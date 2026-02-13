/* environmental_system.h */
#ifndef __ENVIRONMENTAL_SYSTEM_H
#define __ENVIRONMENTAL_SYSTEM_H

#include "main.h"
#include "adc.h"

extern UART_HandleTypeDef huart6;
extern ADC_HandleTypeDef hadc1;
extern uint8_t timer_flag;
extern float alpha;

// ✅ System mode control
typedef enum {
    MODE_AUTO = 0,     // Automatic mode: sensor-based control
    MODE_MANUAL = 1    // Manual mode: command-based control
} SystemMode_t;

extern SystemMode_t g_system_mode;
extern uint8_t g_manual_motor_speed;

/* Function Prototypes */
void Send_Data_to_RaspberryPi(float value, char* sensor_type);
void Run_environmental_system_cycle(void);
void Update_Motor_Control(float co2, float co);
void Update_Remote_Monitoring(float co2, float co);

#endif
