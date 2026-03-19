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

#define AUDIO_STREAM_RX_BLOCK_SIZE   960U
#define AUDIO_STREAM_RING_SIZE       8192U

static uint8_t s_spiRxBuf[AUDIO_STREAM_RX_BLOCK_SIZE];
static uint8_t s_ringBuf[AUDIO_STREAM_RING_SIZE];

static volatile uint32_t s_write = 0;
static volatile uint32_t s_read  = 0;
static volatile uint32_t s_count = 0;

static void AudioStreamRx_Push(const uint8_t *src, uint32_t len);

void AudioStreamRx_Init(void)
{
    s_write = 0;
    s_read = 0;
    s_count = 0;

    memset(s_spiRxBuf, 0, sizeof(s_spiRxBuf));
    memset(s_ringBuf, 0, sizeof(s_ringBuf));
}

void AudioStreamRx_Reset(void)
{
    __disable_irq();
    s_write = 0;
    s_read = 0;
    s_count = 0;
    __enable_irq();
}

void AudioStreamRx_Start(void)
{
    if (HAL_SPI_Receive_DMA(&hspi1, s_spiRxBuf, AUDIO_STREAM_RX_BLOCK_SIZE) != HAL_OK)
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

static void AudioStreamRx_Push(const uint8_t *src, uint32_t len)
{
    uint32_t i;

    for (i = 0; i < len; i++)
    {
        if (s_count >= AUDIO_STREAM_RING_SIZE)
        {
            /* overflow: 가장 단순하게는 뒤 데이터 버림 */
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
        static uint32_t rxCount = 0;

        if (hspi == &hspi1)
        {
            rxCount++;

            if ((rxCount % 20U) == 0U)
            {
                printf("RX done first16: ");
                for (int i = 0; i < 16; i++)
                {
                    printf("%02X ", s_spiRxBuf[i]);
                }
                printf("... len=%u\r\n", (unsigned int)AUDIO_STREAM_RX_BLOCK_SIZE);
            }

            AudioStreamRx_Push(s_spiRxBuf, AUDIO_STREAM_RX_BLOCK_SIZE);

            memset(s_spiRxBuf, 0, sizeof(s_spiRxBuf));

            if (HAL_SPI_Receive_DMA(&hspi1, s_spiRxBuf, AUDIO_STREAM_RX_BLOCK_SIZE) != HAL_OK)
            {
                printf("SPI RX DMA Restart Fail\r\n");
            }
        }
    }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == &hspi1)
    {
        uint32_t err = hspi->ErrorCode;

        printf("SPI RX Error: 0x%08lX ", err);

        if (err & HAL_SPI_ERROR_OVR)  printf("[OVR] ");
        if (err & HAL_SPI_ERROR_MODF) printf("[MODF] ");
        if (err & HAL_SPI_ERROR_CRC)  printf("[CRC] ");
        if (err & HAL_SPI_ERROR_FRE)  printf("[FRE] ");
#ifdef HAL_SPI_ERROR_DMA
        if (err & HAL_SPI_ERROR_DMA)  printf("[DMA] ");
#endif
        printf("\r\n");

        HAL_SPI_Abort(hspi);
        __HAL_SPI_CLEAR_OVRFLAG(hspi);

        memset(s_spiRxBuf, 0, sizeof(s_spiRxBuf));

        if (HAL_SPI_Receive_DMA(&hspi1, s_spiRxBuf, AUDIO_STREAM_RX_BLOCK_SIZE) == HAL_OK)
        {
            printf("SPI RX DMA Restart OK\r\n");
        }
        else
        {
            printf("SPI RX DMA Restart Fail\r\n");
        }
    }
}
