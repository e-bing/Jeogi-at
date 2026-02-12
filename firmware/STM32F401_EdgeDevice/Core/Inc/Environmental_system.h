/* environmental_system.h */
#ifndef __ENVIRONMENTAL_SYSTEM_H
#define __ENVIRONMENTAL_SYSTEM_H

#include "main.h"

extern UART_HandleTypeDef huart6;
extern ADC_HandleTypeDef hadc1;
extern uint8_t timer_flag;
extern float alpha;

/* Function Prototypes */
void Send_Data_to_RaspberryPi(float co2);
void Run_environmental_system_cycle(void);

#endif
