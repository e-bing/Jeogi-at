/* communication.c */
#include "communication.h"
#include <stdio.h>
#include <string.h>

/**
  * @brief  라즈베리파이로 데이터 전송 (JSON 형식)
  * @param  huart: UART 핸들
  * @param  co2: CO2 농도 값
  * @param  co: CO 농도 값
  */
void Send_Data_to_RaspberryPi(UART_HandleTypeDef *huart, float co2, float co)
{
    char json_data[150];

    // JSON 형식으로 데이터 생성 (두 센서 데이터 모두 포함)
    sprintf(json_data, "{\"sensors\":[{\"type\":\"MQ135\",\"co2\":%.2f},{\"type\":\"MQ7\",\"co\":%.2f}]}\r\n",
            co2, co);

    // UART로 전송 (라즈베리파이)
    HAL_UART_Transmit(huart, (uint8_t*)json_data, strlen(json_data), 100);
}
