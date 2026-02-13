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
extern SystemMode_t g_system_mode;

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
    // 1. ❌ Remove the initial uninitialized debug_msg transmission
    // (This was the cause of garbage value like <U+0002>)
    // Delete the existing HAL_UART_Transmit(...) line.

    // 2. ===== Mode control command (processed regardless of order) =====
    if (strstr(json_str, "\"type\":\"mode_control\"")) {
        if (strstr(json_str, "\"action\":\"auto\"")) {
            g_system_mode = MODE_AUTO;
        }
        else if (strstr(json_str, "\"action\":\"manual\"")) {
            g_system_mode = MODE_MANUAL;
        }
        return;
    }

    // 3. ===== Motor control command (manual mode only) =====
    if (strstr(json_str, "\"type\":\"motor_control\"")) {
        // Exit immediately if not in manual mode
        if (g_system_mode != MODE_MANUAL) {
            return;
        }

        // Determine action: check if it is start/on or stop/off
        if (strstr(json_str, "\"action\":\"start\"") || strstr(json_str, "\"action\":\"on\"")) {
            int speed = 100; // Default value

            // Find the position of the "speed" field and parse the number after it
            char *speed_ptr = strstr(json_str, "\"speed\":");
            if (speed_ptr) {
                sscanf(speed_ptr, "\"speed\":%d", &speed);
            }

            g_manual_motor_speed = (speed > 0) ? speed : 100;
            Motor_SetSpeed(g_manual_motor_speed);

            // ❌ Remove plain text transmissions like [MOTOR] ON
            // that cause JSON errors on the server side
        }
        else if (strstr(json_str, "\"action\":\"stop\"") || strstr(json_str, "\"action\":\"off\"")) {
            g_manual_motor_speed = 0;
            Motor_SetSpeed(0);
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
