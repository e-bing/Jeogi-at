/* environmental_system.h */
#ifndef __ENVIRONMENTAL_SYSTEM_H
#define __ENVIRONMENTAL_SYSTEM_H

#include "main.h"
#include "adc.h"

extern UART_HandleTypeDef huart6;
extern ADC_HandleTypeDef hadc1;
extern uint8_t timer_flag;
extern float alpha;

// mode control
typedef enum {
		MODE_AUTO = 0,
		MODE_MANUAL = 1
};
/* Function Prototypes */
void Send_Data_to_RaspberryPi(float value, char* sensor_type);
void Run_environmental_system_cycle(void);

#endif
