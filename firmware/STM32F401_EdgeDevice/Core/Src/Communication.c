/* communication.c */
#include "communication.h"
#include "Motor.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // For sscanf

uint8_t rx_byte;                // Receive 1 byte
char rx_buffer[RX_BUFFER_SIZE]; // Buffer to store until newline (\n)
int rx_index = 0;

extern UART_HandleTypeDef huart6;

// 예찬 확인 해야함
// 1. Start Receive Interrupt (Call once in main.c)
void Start_UART_Receive_IT(UART_HandleTypeDef *huart) {
    HAL_UART_Receive_IT(huart, &rx_byte, 1);
}

// 2. UART Receive Complete Callback (Automatically called upon data reception)
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) { // Check UART instance connected to Raspberry Pi
        if (rx_byte == '\n' || rx_byte == '\r') {
            if (rx_index > 0) {
                rx_buffer[rx_index] = '\0';
                Handle_Received_Command(rx_buffer); // Call command parser
                rx_index = 0; // Reset buffer index
            }
        } else {
            if (rx_index < RX_BUFFER_SIZE - 1) {
                rx_buffer[rx_index++] = rx_byte;
            }
        }
        // Wait for the next 1 byte
        HAL_UART_Receive_IT(huart, &rx_byte, 1);
    }
}

// 3. Parse received JSON command
void Handle_Received_Command(char *json_str) {
    char action[20] = {0};
    int speed = 0;

    if (sscanf(json_str, "{\"type\":\"motor_control\",\"action\":\"%[^\"]\",\"speed\":%d}", action, &speed) >= 2) {
        if (strcmp(action, "start") == 0) {
            // Motor Start Logic (PWM setting, etc.)
            Motor_SetSpeed(100);
        }
        else if (strcmp(action, "stop") == 0) {
            // Motor Stop Logic
            Motor_SetSpeed(0);
        }
    }
}

/**
 * @brief  Sends only the CO2 sensor value to Raspberry Pi via UART6.
 * @note   Simplified version: Send_Data_to_RaspberryPi(value);
 * @param  co2: The CO2 concentration value to transmit.
 */
void Send_Data_to_RaspberryPi(float value, char* sensor_type) {
    char json_data[64]; // Reduced size for single value efficiency

    // JSON Format: {"type":"MQ135","value":400.12}
    // huart6 and "MQ135" are fixed inside to simplify the call.
    sprintf(json_data, "{\"type\":\"%s\",\"value\":%.2f}\r\n", sensor_type, value);

    HAL_UART_Transmit(&huart6, (uint8_t*)json_data, strlen(json_data), 100);
}
