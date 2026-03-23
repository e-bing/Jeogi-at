/*
 * audio_stream_rx.c
 *
 *  Created on: 2026. 3. 19.
 *      Author: 2-21
 */

#include "services/audio/audio_stream_rx.h"
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

/* internal */
static uint32_t AudioStreamRx_PushBlock(const uint8_t *src, uint32_t len);
static uint8_t AudioStreamRx_TakeReadyBlock(uint8_t *outIndex);

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

/*
 * 링 버퍼 -> dst
 * 바이트 단위 루프 대신 최대 2번 memcpy
 */
uint32_t AudioStreamRx_Read(uint8_t *dst, uint32_t len)
{
    uint32_t readLen;
    uint32_t firstChunk;
    uint32_t secondChunk;

    readLen = (len <= s_count) ? len : s_count;

    firstChunk = AUDIO_STREAM_RING_SIZE - s_read;
    if (firstChunk > readLen)
    {
        firstChunk = readLen;
    }

    secondChunk = readLen - firstChunk;

    memcpy(dst, &s_ringBuf[s_read], firstChunk);

    if (secondChunk > 0U)
    {
        memcpy(dst + firstChunk, &s_ringBuf[0], secondChunk);
    }

    s_read += readLen;
    if (s_read >= AUDIO_STREAM_RING_SIZE)
    {
        s_read -= AUDIO_STREAM_RING_SIZE;
    }

    s_count -= readLen;

    return readLen;
}

/*
 * ready된 DMA 블록 하나만 가져옴
 * 한 번 호출에 최대 1블록만 처리하게 하기 위한 함수
 */
static uint8_t AudioStreamRx_TakeReadyBlock(uint8_t *outIndex)
{
    __disable_irq();

    if (s_readyFlags[0] != 0U)
    {
        s_readyFlags[0] = 0;
        *outIndex = 0;
        __enable_irq();
        return 1;
    }

    if (s_readyFlags[1] != 0U)
    {
        s_readyFlags[1] = 0;
        *outIndex = 1;
        __enable_irq();
        return 1;
    }

    __enable_irq();
    return 0;
}

/*
 * src -> 링 버퍼
 * 넣을 수 있는 만큼만 push
 * 최대 2번 memcpy
 */
static uint32_t AudioStreamRx_PushBlock(const uint8_t *src, uint32_t len)
{
    uint32_t freeSize;
    uint32_t pushLen;
    uint32_t firstChunk;
    uint32_t secondChunk;

    freeSize = AUDIO_STREAM_RING_SIZE - s_count;
    pushLen = (len <= freeSize) ? len : freeSize;

    firstChunk = AUDIO_STREAM_RING_SIZE - s_write;
    if (firstChunk > pushLen)
    {
        firstChunk = pushLen;
    }

    secondChunk = pushLen - firstChunk;

    memcpy(&s_ringBuf[s_write], src, firstChunk);

    if (secondChunk > 0U)
    {
        memcpy(&s_ringBuf[0], src + firstChunk, secondChunk);
    }

    s_write += pushLen;
    if (s_write >= AUDIO_STREAM_RING_SIZE)
    {
        s_write -= AUDIO_STREAM_RING_SIZE;
    }

    s_count += pushLen;

    if (pushLen < len)
    {
        s_overflowDropCount += (len - pushLen);
    }

    return pushLen;
}

/*
 * 메인 루프에서 주기적으로 호출
 * - ready된 DMA 버퍼가 있으면 1개만 링 버퍼로 옮김
 * - callback 안에서 무거운 copy 작업을 하지 않기 위한 함수
 */
void AudioStreamRx_Process(void)
{
    uint8_t bufIndex;

    if (AudioStreamRx_TakeReadyBlock(&bufIndex) != 0U)
    {
        AudioStreamRx_PushBlock(s_spiRxBuf[bufIndex], AUDIO_STREAM_RX_BLOCK_SIZE);
    }

    /* 필요할 때만 켜서 확인 */
#if 0
    static uint32_t lastRxBlockCount = 0;
    if ((s_rxBlockCount - lastRxBlockCount) >= 100U)
    {
        lastRxBlockCount = s_rxBlockCount;
        printf("SPI RX blocks=%lu avail=%lu drop=%lu err=%lu\r\n",
               s_rxBlockCount,
               s_count,
               s_overflowDropCount,
               s_spiErrorCount);
    }
#endif
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

        /* 다음 DMA를 최대한 빨리 재시작 */
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
