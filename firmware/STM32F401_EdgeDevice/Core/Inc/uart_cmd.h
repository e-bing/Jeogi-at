#ifndef UART_CMD_H
#define UART_CMD_H

#include "stm32f4xx_hal.h"

void UART_CMD_Init(UART_HandleTypeDef *huart);
void UART_CMD_Process(void);

#endif
