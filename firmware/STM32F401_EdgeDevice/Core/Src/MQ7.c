/* mq7.c */
#include "MQ7.h"

// MQ-7 관련 변수
uint32_t adc_value_co = 0;
float co_ppm = 0;
float ema_co = 0;
uint8_t first_run_co = 1;

// MQ-7 캘리브레이션 상수
float Vc = 5.0f;      // 공급 전압
float RL = 10.0f;     // 부하 저항 10k (센서 보드 사양 확인)
float R0 = 15.0f;     // 깨끗한 공기에서 측정한 Rs 값 (미리 측정해야 함)

/**
  * @brief  MQ-7 초기화
  */
void MQ7_Init(void)
{
    first_run_co = 1;
    ema_co = 0;
}

/**
  * @brief  ADC 값을 CO ppm으로 변환 (MQ-7)
  * @param  adc_val: ADC 읽은 값 (0~4095)
  * @retval CO 농도 (ppm)
  */
float ADC_to_CO(uint32_t adc_val)
{
    float voltage = (adc_val / 4095.0f) * 3.3f; // STM32 ADC 전압 (3.3V 기준)

    // 1. 센서 저항 Rs 계산
    if (voltage == 0) return 0;
    float rs = ((3.3f - voltage) / voltage) * RL;

    // 2. Rs/R0 비 계산
    float ratio = rs / R0;

    // 3. 데이터시트 곡선 기반 ppm 계산 (pow 함수 사용)
    float co_ppm = 98.322f * pow(ratio, -1.458f);

    return co_ppm;
}

/**
  * @brief  MQ-7 CO 센서 읽기 (오버샘플링 + EMA 필터)
  * @param  hadc: ADC 핸들
  * @param  alpha: EMA 필터 계수
  * @retval 필터링된 CO 값 (ppm)
  */
float MQ7_ReadCO(ADC_HandleTypeDef *hadc, float alpha)
{
    // 1. 오버샘플링 (32번 평균)
    uint32_t adc_sum_co = 0;

    // ADC 채널 1로 설정
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
        HAL_Delay(1); // ADC 안정화
    }
    adc_value_co = adc_sum_co / 32;

    // 2. 원시 값 PPM 변환
    float current_co = ADC_to_CO(adc_value_co);

    // 3. EMA 필터 적용
    if(first_run_co) {
        ema_co = current_co;
        first_run_co = 0;
    } else {
        ema_co = (current_co * alpha) + (ema_co * (1.0f - alpha));
    }

    return ema_co;
}
