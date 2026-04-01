/* mq135.c */
#include "features/sensors/mq135.h"
#include "features/sensors/sensor_app.h"
#include <math.h>

// MQ-135 Related Variables
uint32_t adc_value_co2 = 0;
float co2_ppm = 0;
float ema_co2 = 0;

uint8_t first_run_co2 = 1;

// MQ-135 calibration/tuning constants
// Input condition: sensor module at 5V, A0 to STM32 through 10k/10k divider
static const float ADC_VREF = 3.3f;
static const float ADC_DIVIDER_GAIN = 2.0f; // 10k/10k divider compensation
static const float VC = 5.0f;
static const float RL_KOHM = 10.0f;
static const float R0_KOHM = 20.0f; // tune per sensor board/environment
static const float CURVE_A = 116.6020682f;
static const float CURVE_B = -2.769034857f;
static const float CAL_GAIN = 5.0f;   // field tuning gain
static const float CAL_OFFSET = 0.0f; // field tuning offset (ppm)
static const float MODEL_MIN_VALID_PPM = 100.0f;
static const float FALLBACK_LINEAR_PPM_PER_V = 1200.0f; // empirical fallback

static const float CO2_MAX_CLAMP_PPM = 5000.0f;

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
    float adc_voltage = (adc_val / 4095.0f) * ADC_VREF;
    float sensor_voltage = adc_voltage * ADC_DIVIDER_GAIN;
    float rs;
    float ratio;
    float co2_model;
    float co2;

    if (sensor_voltage <= 0.01f)
    {
        return 0.0f;
    }
    if (sensor_voltage >= (VC - 0.01f))
    {
        sensor_voltage = VC - 0.01f;
    }

    // Rs = RL * (Vc - Vout) / Vout
    rs = ((VC - sensor_voltage) / sensor_voltage) * RL_KOHM;
    if (rs <= 0.01f)
    {
        rs = 0.01f;
    }

    // MQ-135 logarithmic curve model
    ratio = rs / R0_KOHM;
    if (ratio <= 0.001f)
    {
        ratio = 0.001f;
    }
    co2_model = CURVE_A * powf(ratio, CURVE_B);

    // If model result is unrealistically low, use voltage-based fallback
    if (!isfinite(co2_model) || (co2_model < MODEL_MIN_VALID_PPM))
    {
        co2 = sensor_voltage * FALLBACK_LINEAR_PPM_PER_V;
    }
    else
    {
        co2 = co2_model;
    }

    co2 = (co2 * CAL_GAIN) + CAL_OFFSET;
    if (co2 < 0.0f)
    {
        co2 = 0.0f;
    }

    if (co2 > CO2_MAX_CLAMP_PPM)
    {
        co2 = CO2_MAX_CLAMP_PPM;
    }
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

    for (int i = 0; i < 32; i++)
    {
        HAL_ADC_Start(hadc);
        if (HAL_ADC_PollForConversion(hadc, 10) == HAL_OK)
        {
            adc_sum_co2 += HAL_ADC_GetValue(hadc);
        }
        HAL_ADC_Stop(hadc);
        HAL_Delay(1); // ADC stabilization
    }
    adc_value_co2 = adc_sum_co2 / 32;

    // 2. Convert raw value to PPM
    float current_co2 = ADC_to_CO2(adc_value_co2);

    // 3. Apply EMA Filter
    if (first_run_co2)
    {
        ema_co2 = current_co2;
        first_run_co2 = 0;
    }
    else
    {
        ema_co2 = (current_co2 * alpha) + (ema_co2 * (1.0f - alpha));
    }

    return ema_co2;
}

void MQ135_GetProtocolData(uint8_t *buffer)
{
    float co2_val = MQ135_ReadCO2(&hadc1, SENSOR_ALPHA);
    uint16_t ppm = (uint16_t)co2_val;

    buffer[0] = (uint8_t)(ppm >> 8);
    buffer[1] = (uint8_t)(ppm & 0xFF);
}
