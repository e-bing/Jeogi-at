#include "Environmental_system.h"
#include "MQ135.h"
#include "MQ7.h"
#include "Motor.h"
#include <stdio.h>
#include <string.h>

/**
  * @brief  Updates the remote monitoring station.
  * @details Currently configured to send CO2 data to the Raspberry Pi.
  * @param  co2: Filtered CO2 concentration in ppm.
  * @param  co:  Filtered CO concentration in ppm (unused for transmission currently).
  */
void Update_Remote_Monitoring(float co2, float co) {
    Send_Data_to_RaspberryPi(co2);
}

/**
  * @brief  Adjusts motor speed based on air quality thresholds.
  * @details Implements safety logic:
  * - High danger (>600ppm CO2 or >25ppm CO): 100% Speed
  * - Warning level (>400ppm CO2 or >9ppm CO): 60% Speed
  * - Normal: Motor Stop
  * @param  co2: Filtered CO2 concentration.
  * @param  co:  Filtered CO concentration.
  */
void Update_Motor_Control(float co2, float co) {
    if(co2 > 600.0f || co > 25.0f) {
        Motor_SetSpeed(100);
    }
    else if (co2 > 400.0f || co > 9.0f) {
        Motor_SetSpeed(60);
    }
    else {
        Motor_SetSpeed(0);
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
    }
}
