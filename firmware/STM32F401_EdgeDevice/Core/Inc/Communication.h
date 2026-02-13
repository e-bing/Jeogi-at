/* communication.h */
#ifndef __COMMUNICATION_H
#define __COMMUNICATION_H

#include "main.h"

#define RX_BUFFER_SIZE 128

extern uint8_t rx_byte;
extern char rx_buffer[RX_BUFFER_SIZE];
extern int rx_index;

/**
  * @brief  Sends sensor data to Raspberry Pi in JSON format.
  * @param  huart: UART handle used for transmission.
  * @param  co2: Measured CO2 concentration value.
  * @param  co: Measured CO concentration value.
  */
void Send_Data_to_RaspberryPi(float value, char* sensor_type);

/**
  * @brief  Starts UART reception in Interrupt mode.
  * @param  huart: UART handle to enable reception.
  */
void Start_UART_Receive_IT(UART_HandleTypeDef *huart);

/**
  * @brief  Parses the received JSON string and executes commands.
  * @param  json_str: Pointer to the buffer containing the received JSON data.
  */
void Handle_Received_Command(char *json_str);

#endif
