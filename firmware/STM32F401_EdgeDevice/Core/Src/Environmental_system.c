#include "Environmental_system.h"
#include "MQ135.h"
#include "MQ7.h"

/**
 * @brief Sends CO2 and CO sensor data to Raspberry Pi via UART.
 * @param co2 Filtered CO2 concentration in ppm.
 * @param co  Filtered CO concentration in ppm.
 */
void Update_Remote_Monitoring(float co2, float co) {
    Send_Data_to_RaspberryPi(co2, "MQ135");
    HAL_Delay(100);
    Send_Data_to_RaspberryPi(co, "MQ7");
    HAL_Delay(100);
}

/**
 * @brief Main task manager for the environmental monitoring system.
 * @note  Designed to be called within the infinite while loop.
 *        Acquires sensor data and transmits every 1 second.
 */
void Run_environmental_system_cycle(void) {
    if (timer_flag == 1) {
        timer_flag = 0;

        float current_co2 = MQ135_ReadCO2(&hadc1, alpha);
        float current_co  = MQ7_ReadCO(&hadc1, alpha);

        Update_Remote_Monitoring(current_co2, current_co);
    }
}
