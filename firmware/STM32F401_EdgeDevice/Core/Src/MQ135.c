/* mq135.c */
#include "MQ135.h"

// MQ-135 관련 변수
uint32_t adc_value_co2 = 0;
float co2_ppm = 0;
float ema_co2 = 0;
uint8_t first_run_co2 = 1;

/**
  * @brief  MQ-135 초기화
  */
void MQ135_Init(void)
{
    first_run_co2 = 1;
    ema_co2 = 0;
}

/**
  * @brief  ADC 값을 CO2 ppm으로 변환 (MQ-135)
  * @param  adc_val: ADC 읽은 값 (0~4095)
  * @retval CO2 농도 (ppm)
  */
float ADC_to_CO2(uint32_t adc_val)
{
    float voltage = (adc_val / 4095.0f) * 3.3f;  // 12bit ADC, 3.3V 기준

    // MQ-135 변환 공식 (실제 환경에서 캘리브레이션 필요)
    // 간단한 선형 근사식 사용
    float co2 = voltage * 1000.0f;  // 예시 변환식

    // 실제 사용시 아래와 같이 보정 필요
    // float rs_r0 = voltage / 1.0;  // R0는 깨끗한 공기에서 측정
    // co2 = 116.6 * pow(rs_r0, -2.769);

    return co2;
}

/**
  * @brief  MQ-135 CO2 센서 읽기 (오버샘플링 + EMA 필터)
  * @param  hadc: ADC 핸들
  * @param  alpha: EMA 필터 계수
  * @retval 필터링된 CO2 값 (ppm)
  */
float MQ135_ReadCO2(ADC_HandleTypeDef *hadc, float alpha)
{
    // 1. 오버샘플링 (32번 평균)
    uint32_t adc_sum_co2 = 0;

    // ADC 채널 0으로 설정
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
        HAL_Delay(1); // ADC 안정화
    }
    adc_value_co2 = adc_sum_co2 / 32;

    // 2. 원시 값 PPM 변환
    float current_co2 = ADC_to_CO2(adc_value_co2);

    // 3. EMA 필터 적용
    if(first_run_co2) {
        ema_co2 = current_co2;
        first_run_co2 = 0;
    } else {
        ema_co2 = (current_co2 * alpha) + (ema_co2 * (1.0f - alpha));
    }

    return ema_co2;
}
