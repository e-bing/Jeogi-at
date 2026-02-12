/* mq7.h */
#ifndef MQ7_H
#define MQ7_H

#include "main.h"
#include <math.h>

// MQ-7 Related Variables
extern uint32_t adc_value_co;
extern float co_ppm;
extern float ema_co;
extern uint8_t first_run_co;

// MQ-7 Function Prototypes
void MQ7_Init(void);
float MQ7_ReadCO(ADC_HandleTypeDef *hadc, float alpha);
float ADC_to_CO(uint32_t adc_val);

#endif /* MQ7_H */
