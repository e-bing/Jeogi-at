/* mq135.c */
#include "MQ135.h"

// MQ-135 Related Variables
uint32_t adc_value_co2 = 0;
float co2_ppm = 0;
float ema_co2 = 0;

uint8_t first_run_co2 = 1;

/**
  * @brief  MQ-135 Initialization
  */
void MQ135_Init(void)
{
    first_run_co2 = 1;
    ema_co2 = 0;
}

/**
  * @brief  Convert ADC value to CO2 ppm (MQ-135)
  * @param  adc_val: Read ADC value (0~4095)
  * @retval CO2 concentration (ppm)
  */
float ADC_to_CO2(uint32_t adc_val)
{
    float voltage = (adc_val / 4095.0f) * 3.3f;  // 12-bit ADC, 3.3V reference

    // MQ-135 conversion formula (Calibration required for actual environment)
    // Using a simple linear approximation
    float co2 = voltage * 1000.0f;  // Example conversion formula

    // Calibration required as follows for actual use:
    // float rs_r0 = voltage / 1.0;  // R0 is measured in clean air
    // co2 = 116.6 * pow(rs_r0, -2.769);

    return co2;
}

/**
  * @brief  Read MQ-135 CO2 sensor (Oversampling + EMA Filter)
  * @param  hadc: ADC handle
  * @param  alpha: EMA filter coefficient
  * @retval Filtered CO2 value (ppm)
  */
float MQ135_ReadCO2(ADC_HandleTypeDef *hadc, float alpha)
{
    // 1. Oversampling (Average of 32 samples)
    uint32_t adc_sum_co2 = 0;

    // Set to ADC Channel 0
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
    HAL_ADC_ConfigChannel(hadc, &sConfig);

    for(int i = 0; i < 32; i++) {
        HAL_ADC_Start(hadc);
        if(HAL_ADC_PollForConversion(hadc, 10) == HAL_OK) {
            adc_sum_co2 += HAL_ADC_GetValue(hadc);
        }
        HAL_ADC_Stop(hadc);
        HAL_Delay(1); // ADC stabilization
    }
    adc_value_co2 = adc_sum_co2 / 32;

    // 2. Convert raw value to PPM
    float current_co2 = ADC_to_CO2(adc_value_co2);

    // 3. Apply EMA Filter
    if(first_run_co2) {
        ema_co2 = current_co2;
        first_run_co2 = 0;
    } else {
        ema_co2 = (current_co2 * alpha) + (ema_co2 * (1.0f - alpha));
    }

    return ema_co2;
}
