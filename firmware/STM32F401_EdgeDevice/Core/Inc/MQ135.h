/* mq135.h */
#ifndef MQ135_H
#define MQ135_H

#include "main.h"

/* MQ-135 Sensor Variables */
extern uint32_t adc_value_co2;  // Raw ADC value for CO2
extern float co2_ppm;           // Calculated CO2 concentration in ppm
extern float ema_co2;           // Exponential Moving Average of CO2
extern uint8_t first_run_co2;   // Flag to initialize EMA on the first run

/* MQ-135 Function Prototypes */

/**
  * @brief  Initializes the MQ-135 sensor parameters.
  */
void MQ135_Init(void);

/**
  * @brief  Reads the CO2 level from the ADC and applies an EMA filter.
  * @param  hadc: ADC handle used for the measurement.
  * @param  alpha: Smoothing factor for the EMA filter (0.0 to 1.0).
  * @retval Filtered CO2 concentration value.
  */
float MQ135_ReadCO2(ADC_HandleTypeDef *hadc, float alpha);

/**
  * @brief  Converts a raw ADC value to CO2 ppm concentration.
  * @param  adc_val: Raw 12-bit or 10-bit ADC integer value.
  * @retval Calculated CO2 ppm as a float.
  */
float ADC_to_CO2(uint32_t adc_val);

void MQ135_GetProtocolData(uint8_t* buffer);

#endif /* MQ135_H */
