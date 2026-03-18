#include "main.h"
#include "spi.h"
#include "services/audio_player.h"
#include <stdio.h>
#include <string.h>

#define SPI_STREAM_DMA_SIZE   1024U
#define SPI_STREAM_HALF_SIZE  (SPI_STREAM_DMA_SIZE / 2U)

static uint8_t g_spiStreamDmaBuf[SPI_STREAM_DMA_SIZE];
volatile uint8_t g_spiStreamRxBusy = 0;

volatile uint32_t g_spiRxHalfCnt = 0;
volatile uint32_t g_spiRxDoneCnt = 0;
volatile uint32_t g_spiErrCnt = 0;
volatile uint32_t g_spiLastErr = 0;
volatile uint32_t g_spiDroppedBytes = 0;

void StreamRx_Start(void)
{
    if (g_spiStreamRxBusy)
    {
        return;
    }

    memset(g_spiStreamDmaBuf, 0, sizeof(g_spiStreamDmaBuf));
    g_spiStreamRxBusy = 1;

    if (HAL_SPI_Receive_DMA(&hspi1, g_spiStreamDmaBuf, SPI_STREAM_DMA_SIZE) != HAL_OK)
    {
        g_spiStreamRxBusy = 0;
        printf("SPI1 DMA Receive Start Fail\r\n");
    }
}

void HAL_SPI_RxHalfCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == &hspi1)
    {
        uint32_t written;

        g_spiRxHalfCnt++;

        /* 앞 절반을 오디오 링버퍼에 적재 */
        written = Audio_StreamWrite(&g_spiStreamDmaBuf[0], SPI_STREAM_HALF_SIZE);
        if (written < SPI_STREAM_HALF_SIZE)
        {
            g_spiDroppedBytes += (SPI_STREAM_HALF_SIZE - written);
        }
    }
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == &hspi1)
    {
        uint32_t written;

        g_spiRxDoneCnt++;

        /* 뒤 절반을 오디오 링버퍼에 적재 */
        written = Audio_StreamWrite(&g_spiStreamDmaBuf[SPI_STREAM_HALF_SIZE], SPI_STREAM_HALF_SIZE);
        if (written < SPI_STREAM_HALF_SIZE)
        {
            g_spiDroppedBytes += (SPI_STREAM_HALF_SIZE - written);
        }

        g_spiStreamRxBusy = 0;
        StreamRx_Start();
    }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == &hspi1)
    {
        g_spiErrCnt++;
        g_spiLastErr = hspi->ErrorCode;
        g_spiStreamRxBusy = 0;

        __HAL_SPI_CLEAR_OVRFLAG(&hspi1);
        HAL_SPI_DMAStop(&hspi1);

        StreamRx_Start();
    }
}
