/* mq7.h */
#ifndef FEATURES_SENSORS_MQ7_H
#define FEATURES_SENSORS_MQ7_H

#include "main.h"
#include <math.h>

#include "adc.h" // ADC_HandleTypeDef 정의를 위해 필요

// MQ-7 Related Variables
extern uint32_t adc_value_co;
extern float co_ppm;
extern float ema_co;
extern uint8_t first_run_co;
extern ADC_HandleTypeDef hadc1;

// MQ-7 Function Prototypes
void MQ7_Init(void);
float MQ7_ReadCO(ADC_HandleTypeDef *hadc, float alpha);
float ADC_to_CO(uint32_t adc_val);
void MQ7_GetProtocolData(uint8_t* buffer);

#endif /* FEATURES_SENSORS_MQ7_H */
