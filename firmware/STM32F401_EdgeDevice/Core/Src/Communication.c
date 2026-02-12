/* communication.c */
#include "communication.h"
#include "Motor.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // For sscanf

uint8_t rx_byte;                // Receive 1 byte
char rx_buffer[RX_BUFFER_SIZE]; // Buffer to store until newline (\n)
int rx_index = 0;

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

// Existing transmission function
void Send_Data_to_RaspberryPi(UART_HandleTypeDef *huart, float co2, float co) {
    char json_data[150];
    sprintf(json_data, "{\"sensors\":[{\"type\":\"MQ135\",\"co2\":%.2f},{\"type\":\"MQ7\",\"co\":%.2f}]}\r\n", co2, co);
    HAL_UART_Transmit(huart, (uint8_t*)json_data, strlen(json_data), 100);
}
