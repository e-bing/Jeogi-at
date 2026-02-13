#include "Environmental_system.h"
#include "MQ135.h"
#include "MQ7.h"
#include "Motor.h"
#include <stdio.h>
#include <string.h>

// ✅ Global mode variables
SystemMode_t g_system_mode = MODE_AUTO;  // Default: Auto mode
uint8_t g_manual_motor_speed = 0;

/**
  * @brief  Updates the remote monitoring station.
  * @details Sends CO2 and CO data to Raspberry Pi via UART.
  * @param  co2: Filtered CO2 concentration in ppm.
  * @param  co:  Filtered CO concentration in ppm.
  */
void Update_Remote_Monitoring(float co2, float co) {
    Send_Data_to_RaspberryPi(co2, "MQ135");
    HAL_Delay(100);  // ✅ 50ms delay to prevent buffer overflow
    Send_Data_to_RaspberryPi(co, "MQ7");
    HAL_Delay(100);  // ✅ 50ms delay
}

/**
  * @brief  Adjusts motor speed based on air quality thresholds.
  * @details Implements safety logic in AUTO mode only:
  * - High danger (>600ppm CO2 or >25ppm CO): 100% Speed
  * - Warning level (>400ppm CO2 or >9ppm CO): 60% Speed
  * - Normal: Motor Stop
  * @param  co2: Filtered CO2 concentration.
  * @param  co:  Filtered CO concentration.
  */
void Update_Motor_Control(float co2, float co) {
    // ✅ Only control motor in AUTO mode
    if (g_system_mode != MODE_AUTO) {
        return;
    }

    printf("[AUTO] CO2: %.1f ppm, CO: %.1f ppm", co2, co);

    if(co2 > 600.0f || co > 25.0f) {
        Motor_SetSpeed(100);
        printf(" -> Motor: 100%%\r\n");
    }
    else if (co2 > 300.0f || co > 9.0f) {
        Motor_SetSpeed(60);
        printf(" -> Motor: 60%%\r\n");
    }
    else {
        Motor_SetSpeed(0);
        printf(" -> Motor: OFF\r\n");
    }
}

/**
  * @brief  Main task manager for the environmental monitoring system.
  * @details Orchestrates sensor acquisition and logic execution every 1 second.
  * This function is designed to be called within the infinite while loop.
  */
void Run_environmental_system_cycle(void) {
    /* Execute logic only when the 1-second timer flag is set */
    if (timer_flag == 1) {
        timer_flag = 0; // Reset flag for the next cycle

        /* 1. Sensor Data Acquisition */
        float current_co2 = MQ135_ReadCO2(&hadc1, alpha);
        float current_co = MQ7_ReadCO(&hadc1, alpha);

        /* 2. Task Execution */
        Update_Remote_Monitoring(current_co2, current_co);
        Update_Motor_Control(current_co2, current_co);

        // ✅ In MANUAL mode, maintain current motor speed
        if (g_system_mode == MODE_MANUAL) {
            Motor_SetSpeed(g_manual_motor_speed);
        }
    }
}
