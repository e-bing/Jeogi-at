/*
 * audio_streaming.c
 *
 *  Created on: 2026. 3. 17.
 *      Author: 2-21
 */


#include "main.h"
#include "spi.h"
#include "services/audio_player.h"
#include <stdio.h>

#define SPI_STREAM_RX_SIZE   1920U

static uint8_t g_spiStreamRxBuf[SPI_STREAM_RX_SIZE];
static volatile uint8_t g_spiStreamRxBusy = 0;

void StreamRx_Start(void)
{
    if (g_spiStreamRxBusy)
    {
        return;
    }

    g_spiStreamRxBusy = 1;

    if (HAL_SPI_Receive_IT(&hspi1, g_spiStreamRxBuf, SPI_STREAM_RX_SIZE) != HAL_OK)
    {
        g_spiStreamRxBusy = 0;
        printf("SPI1 Receive Start Fail\r\n");
    }
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == &hspi1)
    {
        g_spiStreamRxBusy = 0;

        Audio_StreamWrite(g_spiStreamRxBuf, SPI_STREAM_RX_SIZE);

        StreamRx_Start();
    }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == &hspi1)
    {
        g_spiStreamRxBusy = 0;
        printf("SPI1 Error: 0x%08lX\r\n", hspi->ErrorCode);

        StreamRx_Start();
    }
}
