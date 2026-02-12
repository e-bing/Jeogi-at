#include "motor.h"
#include "tim.h"

// TIM4 채널 2 (PB7) 사용 기준 (사용자 설정에 따라 채널 확인 필요)
extern TIM_HandleTypeDef htim4;

void Motor_Init(void) {
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2); // PB7은 보통 TIM4_CH2
    Motor_SetSpeed(MOTOR_STOP);
}

void Motor_SetSpeed(uint8_t speed_percent) {
    // 100이 넘는 값이 들어오면 100으로 제한 (방어적 설계)
    if (speed_percent > 100) speed_percent = 100;

    uint32_t period = __HAL_TIM_GET_AUTORELOAD(&htim4);

    // 입력된 %에 따라 Pulse 값 계산 (예: 60 입력 시 period의 0.6배)
    uint32_t pulse = (uint32_t)((period + 1) * (speed_percent / 100.0f));

    if (speed_percent == 0) {
        // 정지 상태
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_RESET);
    } else {
        // 회전 상태 (정방향 기준)
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_RESET);
    }

    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, pulse);
}
