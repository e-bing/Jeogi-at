/* communication.c */
#include "Communication.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

uint8_t rx_byte;
char rx_buffer[RX_BUFFER_SIZE];
int rx_index = 0;

extern UART_HandleTypeDef huart6;

/**
 * @brief Starts UART interrupt reception
 * @note  Call once in main.c after initialization
 */
void Start_UART_Receive_IT(UART_HandleTypeDef *huart) {
    HAL_UART_Receive_IT(huart, &rx_byte, 1);
}

/**
 * @brief UART Receive Complete Callback
 * @note  Automatically called by HAL when 1 byte is received
 *        STM32 only receives data - mode/motor control handled by Raspberry Pi
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART6) {
        if (rx_byte == '\n' || rx_byte == '\r') {
            if (rx_index > 0) {
                rx_buffer[rx_index] = '\0';
                rx_index = 0;
            }
        } else {
            if (rx_index < RX_BUFFER_SIZE - 1) {
                rx_buffer[rx_index++] = rx_byte;
            }
        }
        HAL_UART_Receive_IT(huart, &rx_byte, 1);
    }
}

/**
 * @brief Sends sensor data to Raspberry Pi via UART6
 * @note  Transmits in JSON format: {"type":"sensor_name","value":123.45}
 * @param value       Sensor reading value
 * @param sensor_type Sensor type identifier ("MQ135" or "MQ7")
 */
void Send_Data_to_RaspberryPi(float value, char* sensor_type) {
    char json_data[64];
    sprintf(json_data, "{\"type\":\"%s\",\"value\":%.2f}\r\n", sensor_type, value);
    HAL_Delay(50);
    HAL_UART_Transmit(&huart6, (uint8_t*)json_data, strlen(json_data), 100);
    HAL_Delay(50);
}
