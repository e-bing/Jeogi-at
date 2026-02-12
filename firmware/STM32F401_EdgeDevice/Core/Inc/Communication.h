/* communication.h */
#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include "main.h"

// 통신 함수 선언
void Send_Data_to_RaspberryPi(UART_HandleTypeDef *huart, float co2, float co);

#endif /* COMMUNICATION_H */
