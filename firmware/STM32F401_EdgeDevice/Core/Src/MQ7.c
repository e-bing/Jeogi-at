/* mq7.c */
#include "MQ7.h"

// MQ-7 Related Variables
uint32_t adc_value_co = 0;
float co_ppm = 0;
float ema_co = 0;
uint8_t first_run_co = 1;

// MQ-7 Calibration Constants
float Vc = 5.0f;      // Supply voltage
float RL = 10.0f;     // Load resistance 10k (Verify sensor board specifications)
float R0 = 15.0f;     // Rs value measured in clean air (Must be pre-measured)

/**
  * @brief  MQ-7 Initialization
  */
void MQ7_Init(void)
{
    first_run_co = 1;
    ema_co = 0;
}

/**
  * @brief  Convert ADC value to CO ppm (MQ-7)
  * @param  adc_val: Read ADC value (0~4095)
  * @retval CO concentration (ppm)
  */
float ADC_to_CO(uint32_t adc_val)
{
    float voltage = (adc_val / 4095.0f) * 3.3f; // STM32 ADC voltage (3.3V reference)

    // 1. Calculate sensor resistance Rs
    if (voltage == 0) return 0;
    float rs = ((3.3f - voltage) / voltage) * RL;

    // 2. Calculate Rs/R0 ratio
    float ratio = rs / R0;

    // 3. Calculate ppm based on datasheet curve (Using pow function)
    float co_ppm = 98.322f * pow(ratio, -1.458f);

    return co_ppm;
}

/**
  * @brief  Read MQ-7 CO sensor (Oversampling + EMA Filter)
  * @param  hadc: ADC handle
  * @param  alpha: EMA filter coefficient
  * @retval Filtered CO value (ppm)
  */
float MQ7_ReadCO(ADC_HandleTypeDef *hadc, float alpha)
{
    // 1. Oversampling (Average of 32 samples)
    uint32_t adc_sum_co = 0;

    // Configure to ADC Channel 1
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_1;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
    HAL_ADC_ConfigChannel(hadc, &sConfig);

    for(int i = 0; i < 32; i++) {
        HAL_ADC_Start(hadc);
        if(HAL_ADC_PollForConversion(hadc, 10) == HAL_OK) {
            adc_sum_co += HAL_ADC_GetValue(hadc);
        }
        HAL_ADC_Stop(hadc);
        HAL_Delay(1); // ADC stabilization
    }
    adc_value_co = adc_sum_co / 32;

    // 2. Convert raw value to PPM
    float current_co = ADC_to_CO(adc_value_co);

    // 3. Apply EMA Filter
    if(first_run_co) {
        ema_co = current_co;
        first_run_co = 0;
    } else {
        ema_co = (current_co * alpha) + (ema_co * (1.0f - alpha));
    }

    return ema_co;
}
