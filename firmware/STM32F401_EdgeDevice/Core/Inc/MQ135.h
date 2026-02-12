/* mq135.h */
#ifndef MQ135_H
#define MQ135_H

#include "main.h"

// MQ-135 관련 변수
extern uint32_t adc_value_co2;
extern float co2_ppm;
extern float ema_co2;
extern uint8_t first_run_co2;

// MQ-135 함수 선언
void MQ135_Init(void);
float MQ135_ReadCO2(ADC_HandleTypeDef *hadc, float alpha);
float ADC_to_CO2(uint32_t adc_val);

#endif /* MQ135_H */
