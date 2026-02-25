#ifndef UART_HANDLER_H
#define UART_HANDLER_H

#include "usart.h"
#include <stdint.h>

void UART_CMD_Init(UART_HandleTypeDef *huart);
void UART_RxCallback(uint8_t byte);   // 인터럽트에서 호출
void UART_Handler_Process(void);      // main 루프에서 호출
void UART_SendACK(uint8_t cmd);
void UART_SendNACK(uint8_t cmd, uint8_t err);
void UART_SendSensorResp(uint8_t cmd, uint8_t *data, uint8_t len);

#endif
