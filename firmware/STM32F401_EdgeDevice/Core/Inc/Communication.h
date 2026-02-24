/* communication.h */
#ifndef __COMMUNICATION_H
#define __COMMUNICATION_H

#include "main.h"

#define RX_BUFFER_SIZE 128

extern uint8_t rx_byte;
extern char rx_buffer[RX_BUFFER_SIZE];
extern int rx_index;

/**
 * @brief Sends sensor data to Raspberry Pi in JSON format.
 * @param value       Sensor reading value.
 * @param sensor_type Sensor type identifier ("MQ135" or "MQ7").
 */
void Send_Data_to_RaspberryPi(float value, char* sensor_type);

/**
 * @brief Starts UART reception in interrupt mode.
 * @param huart UART handle to enable reception.
 */
void Start_UART_Receive_IT(UART_HandleTypeDef *huart);

#endif
