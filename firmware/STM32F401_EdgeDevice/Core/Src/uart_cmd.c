#include "uart_cmd.h"
#include "dev_ctrl.h"
#include <string.h>
#include <stdio.h>

static UART_HandleTypeDef *uart;
static uint8_t rx_buf[64];
static volatile uint8_t rx_flag = 0;

void UART_CMD_Init(UART_HandleTypeDef *huart)
{
    uart = huart;

    HAL_UARTEx_ReceiveToIdle_DMA(
        uart,
        rx_buf,
        sizeof(rx_buf)
    );

    printf("uart init\r\n");
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart,
                                uint16_t Size)
{
//    printf("RX callback! size=%d\r\n", Size);

    if(huart == uart)
    {
        rx_buf[Size] = 0;
        rx_flag = 1;

        HAL_UARTEx_ReceiveToIdle_DMA(
            uart,
            rx_buf,
            sizeof(rx_buf)
        );
    }
}

static void ParseCmd(char *cmd)
{
//	printf("received: %s\r\n", cmd);

    char dev[8];
    char state[8];

    if(sscanf(cmd, "$CMD,%[^,],%[^#]#", dev, state) == 2)
    {
        Sensor_SetState(dev, state);
    }
}

void UART_CMD_Process(void)
{
    if(rx_flag)
    {
        rx_flag = 0;
        ParseCmd((char*)rx_buf);
    }
}
