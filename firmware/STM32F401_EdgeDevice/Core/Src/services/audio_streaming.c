#include "main.h"
#include "spi.h"
#include <stdio.h>
#include <string.h>

#define SPI_STREAM_RX_SIZE 8U

uint8_t g_spiStreamRxBuf[SPI_STREAM_RX_SIZE];
volatile uint8_t g_spiStreamRxBusy = 0;

volatile uint32_t g_spiRxHalfCnt = 0;
volatile uint32_t g_spiRxDoneCnt = 0;
volatile uint32_t g_spiErrCnt = 0;
volatile uint32_t g_spiLastErr = 0;

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
    else
    {
        printf("SPI1 Receive Armed\r\n");
    }
}

void HAL_SPI_RxHalfCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == &hspi1)
    {
        g_spiRxHalfCnt++;
    }
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == &hspi1)
    {
        g_spiRxDoneCnt++;
        g_spiStreamRxBusy = 0;

        /* 다음 수신 재시작 */
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

        /* 에러 후 재시작 */
        StreamRx_Start();
    }
}
