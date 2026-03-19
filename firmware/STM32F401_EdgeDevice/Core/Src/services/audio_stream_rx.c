/*
 * audio_stream_rx.c
 *
 *  Created on: 2026. 3. 19.
 *      Author: 2-21
 */

#include "services/audio_stream_rx.h"
#include "spi.h"
#include <string.h>
#include <stdio.h>

#define AUDIO_STREAM_RX_BLOCK_SIZE   1920U
#define AUDIO_STREAM_RING_SIZE       32768U

static uint8_t s_spiRxBuf[2][AUDIO_STREAM_RX_BLOCK_SIZE];
static uint8_t s_ringBuf[AUDIO_STREAM_RING_SIZE];

static volatile uint32_t s_write = 0;
static volatile uint32_t s_read  = 0;
static volatile uint32_t s_count = 0;

/* ping-pong DMA state */
static volatile uint8_t s_dmaBufIndex = 0;
static volatile uint8_t s_readyFlags[2] = {0, 0};
static volatile uint32_t s_rxBlockCount = 0;
static volatile uint32_t s_overflowDropCount = 0;
static volatile uint32_t s_spiErrorCount = 0;

static void AudioStreamRx_Push(const uint8_t *src, uint32_t len);

void AudioStreamRx_Init(void)
{
    __disable_irq();

    s_write = 0;
    s_read = 0;
    s_count = 0;

    s_dmaBufIndex = 0;
    s_readyFlags[0] = 0;
    s_readyFlags[1] = 0;
    s_rxBlockCount = 0;
    s_overflowDropCount = 0;
    s_spiErrorCount = 0;

    __enable_irq();

    memset(s_spiRxBuf, 0, sizeof(s_spiRxBuf));
    memset(s_ringBuf, 0, sizeof(s_ringBuf));
}

void AudioStreamRx_Reset(void)
{
    __disable_irq();

    s_write = 0;
    s_read = 0;
    s_count = 0;

    s_dmaBufIndex = 0;
    s_readyFlags[0] = 0;
    s_readyFlags[1] = 0;
    s_rxBlockCount = 0;
    s_overflowDropCount = 0;
    s_spiErrorCount = 0;

    __enable_irq();
}

void AudioStreamRx_Start(void)
{
    __disable_irq();
    s_dmaBufIndex = 0;
    s_readyFlags[0] = 0;
    s_readyFlags[1] = 0;
    __enable_irq();

    if (HAL_SPI_Receive_DMA(&hspi1,
                            s_spiRxBuf[s_dmaBufIndex],
                            AUDIO_STREAM_RX_BLOCK_SIZE) != HAL_OK)
    {
        printf("SPI RX DMA Start Fail\r\n");
    }
    else
    {
        printf("SPI RX DMA Start OK\r\n");
    }
}

uint32_t AudioStreamRx_Available(void)
{
    return s_count;
}

uint32_t AudioStreamRx_Read(uint8_t *dst, uint32_t len)
{
    uint32_t i;
    uint32_t readLen;

    __disable_irq();

    readLen = (len <= s_count) ? len : s_count;

    for (i = 0; i < readLen; i++)
    {
        dst[i] = s_ringBuf[s_read];
        s_read = (s_read + 1U) % AUDIO_STREAM_RING_SIZE;
    }

    s_count -= readLen;

    __enable_irq();

    return readLen;
}

/*
 * 메인 루프에서 주기적으로 호출
 * - DMA 완료된 버퍼를 ring buffer로 옮김
 * - callback 안에서 무거운 copy 작업을 하지 않기 위한 함수
 */
void AudioStreamRx_Process(void)
{
    uint8_t localReady0 = 0;
    uint8_t localReady1 = 0;

    __disable_irq();
    localReady0 = s_readyFlags[0];
    localReady1 = s_readyFlags[1];
    s_readyFlags[0] = 0;
    s_readyFlags[1] = 0;
    __enable_irq();

    if (localReady0)
    {
        AudioStreamRx_Push(s_spiRxBuf[0], AUDIO_STREAM_RX_BLOCK_SIZE);
    }

    if (localReady1)
    {
        AudioStreamRx_Push(s_spiRxBuf[1], AUDIO_STREAM_RX_BLOCK_SIZE);
    }

    /* 너무 자주 찍히지 않게 상태 로그 */
    static uint32_t lastRxBlockCount = 0;
    if ((s_rxBlockCount - lastRxBlockCount) >= 50U)
    {
        lastRxBlockCount = s_rxBlockCount;
        printf("SPI RX blocks=%lu avail=%lu drop=%lu err=%lu\r\n",
               s_rxBlockCount,
               s_count,
               s_overflowDropCount,
               s_spiErrorCount);
    }
}

static void AudioStreamRx_Push(const uint8_t *src, uint32_t len)
{
    uint32_t i;

    for (i = 0; i < len; i++)
    {
        if (s_count >= AUDIO_STREAM_RING_SIZE)
        {
            s_overflowDropCount++;
            break;
        }

        s_ringBuf[s_write] = src[i];
        s_write = (s_write + 1U) % AUDIO_STREAM_RING_SIZE;
        s_count++;
    }
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == &hspi1)
    {
        uint8_t doneIndex;
        uint8_t nextIndex;

        doneIndex = s_dmaBufIndex;
        nextIndex = (uint8_t)(doneIndex ^ 1U);

        s_rxBlockCount++;
        s_readyFlags[doneIndex] = 1;
        s_dmaBufIndex = nextIndex;

        /* 핵심: 다음 DMA를 최대한 빨리 재시작 */
        if (HAL_SPI_Receive_DMA(&hspi1,
                                s_spiRxBuf[nextIndex],
                                AUDIO_STREAM_RX_BLOCK_SIZE) != HAL_OK)
        {
            s_spiErrorCount++;
        }
    }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == &hspi1)
    {
        uint32_t err = hspi->ErrorCode;
        s_spiErrorCount++;

        HAL_SPI_Abort(hspi);
        __HAL_SPI_CLEAR_OVRFLAG(hspi);

        __disable_irq();
        s_dmaBufIndex = 0;
        s_readyFlags[0] = 0;
        s_readyFlags[1] = 0;
        __enable_irq();

        if (HAL_SPI_Receive_DMA(&hspi1,
                                s_spiRxBuf[s_dmaBufIndex],
                                AUDIO_STREAM_RX_BLOCK_SIZE) != HAL_OK)
        {
            printf("SPI RX Restart Fail err=0x%08lX\r\n", err);
        }
        else
        {
            printf("SPI RX Restart OK err=0x%08lX\r\n", err);
        }
    }
}
