/* communication.c */
#include "Communication.h"
#include "Motor.h"
#include "Environmental_system.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

uint8_t rx_byte;                // Single byte buffer for interrupt reception
char rx_buffer[RX_BUFFER_SIZE]; // Buffer to accumulate received data until newline
int rx_index = 0;               // Current index in rx_buffer

extern UART_HandleTypeDef huart6;

/**
  * @brief  Starts UART interrupt reception
  * @note   Call this once in main.c after initialization
  * @param  huart: Pointer to UART handle
  */
void Start_UART_Receive_IT(UART_HandleTypeDef *huart) {
    HAL_UART_Receive_IT(huart, &rx_byte, 1);
}

/**
  * @brief  UART Receive Complete Callback
  * @note   Automatically called by HAL when 1 byte is received
  * @param  huart: Pointer to UART handle that triggered the callback
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART6) { // Check if it's UART6 (connected to Raspberry Pi)
        if (rx_byte == '\n' || rx_byte == '\r') {
            // Newline detected - process complete command
            if (rx_index > 0) {
                rx_buffer[rx_index] = '\0'; // Null-terminate the string
                Handle_Received_Command(rx_buffer); // Parse and execute command
                rx_index = 0; // Reset buffer index for next command
            }
        } else {
            // Accumulate received byte into buffer
            if (rx_index < RX_BUFFER_SIZE - 1) {
                rx_buffer[rx_index++] = rx_byte;
            }
        }
        // Re-enable interrupt to receive next byte
        HAL_UART_Receive_IT(huart, &rx_byte, 1);
    }
}

/**
  * @brief  Parses and executes received JSON commands from Raspberry Pi
  * @details Supports two command types:
  *          1. Mode control: {"type":"mode_control","action":"auto"|"manual"}
  *          2. Motor control: {"type":"motor_control","action":"start"|"stop","speed":0-100}
  * @param  json_str: Null-terminated JSON string to parse
  */
void Handle_Received_Command(char *json_str) {
    char debug_msg[128];

    // Debug: Echo received command back to Raspberry Pi
    sprintf(debug_msg, "[RX] %s\r\n", json_str);
    HAL_UART_Transmit(&huart6, (uint8_t*)debug_msg, strlen(debug_msg), 100);

    // ===== Mode Control Command =====
    if (strstr(json_str, "\"type\":\"mode_control\"")) {
        if (strstr(json_str, "\"action\":\"auto\"")) {
            g_system_mode = MODE_AUTO;
            HAL_UART_Transmit(&huart6, (uint8_t*)"[MODE] Auto mode activated\r\n", 28, 100);
        }
        else if (strstr(json_str, "\"action\":\"manual\"")) {
            g_system_mode = MODE_MANUAL;
            HAL_UART_Transmit(&huart6, (uint8_t*)"[MODE] Manual mode activated\r\n", 30, 100);
        }
        return; // Exit after processing mode command
    }

    // ===== Motor Control Command (Manual mode only) =====
    if (strstr(json_str, "\"type\":\"motor_control\"")) {
        // Reject motor commands in auto mode
        if (g_system_mode != MODE_MANUAL) {
            HAL_UART_Transmit(&huart6, (uint8_t*)"[WARN] Auto mode - command ignored\r\n", 37, 100);
            return;
        }

        char action[20] = {0};
        int speed = 0;

        // Parse JSON: extract action and speed fields
        if (sscanf(json_str, "{\"type\":\"motor_control\",\"action\":\"%[^\"]\",\"speed\":%d}", action, &speed) >= 1) {
            if (strcmp(action, "start") == 0 || strcmp(action, "on") == 0) {
                // Motor start command
                g_manual_motor_speed = (speed > 0) ? speed : 100; // Use speed or default to 100
                Motor_SetSpeed(g_manual_motor_speed);

                sprintf(debug_msg, "[MOTOR] ON - Speed: %d%%\r\n", g_manual_motor_speed);
                HAL_UART_Transmit(&huart6, (uint8_t*)debug_msg, strlen(debug_msg), 100);
            }
            else if (strcmp(action, "stop") == 0 || strcmp(action, "off") == 0) {
                // Motor stop command
                g_manual_motor_speed = 0;
                Motor_SetSpeed(0);
                HAL_UART_Transmit(&huart6, (uint8_t*)"[MOTOR] OFF\r\n", 13, 100);
            }
        }
    }
}

/**
  * @brief  Sends sensor data to Raspberry Pi via UART6
  * @details Transmits data in JSON format: {"type":"sensor_name","value":123.45}
  * @param  value: Sensor reading value
  * @param  sensor_type: Sensor type identifier ("MQ135" or "MQ7")
  */
void Send_Data_to_RaspberryPi(float value, char* sensor_type) {
    char json_data[64];

    // Format JSON string
    sprintf(json_data, "{\"type\":\"%s\",\"value\":%.2f}\r\n", sensor_type, value);

    // Delay before transmission to prevent buffer overflow
    HAL_Delay(50);

    // Transmit JSON data via UART6
    HAL_UART_Transmit(&huart6, (uint8_t*)json_data, strlen(json_data), 100);

    // Delay after transmission for stability
    HAL_Delay(50);
}
