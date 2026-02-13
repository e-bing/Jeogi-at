#include "motor.h"
#include "tim.h"

// Using TIM4 Channel 2 (PB7) as a reference (Verify channel configuration in CubeMX)
extern TIM_HandleTypeDef htim4;

/**
  * @brief  Initializes the motor by starting PWM and setting speed to STOP.
  */
void Motor_Init(void) {
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2); // PB7 is typically mapped to TIM4_CH2
    Motor_SetSpeed(0);
}

/**
  * @brief  Sets the motor speed and direction.
  * @param  speed_percent: Speed value from 0 to 100.
  */
void Motor_SetSpeed(uint8_t speed_percent) {
    // Limit the input value to 100 for safety (defensive programming)
    if (speed_percent > 100) speed_percent = 100;

    // Get the Auto-Reload Register (ARR) value of the timer
    uint32_t period = __HAL_TIM_GET_AUTORELOAD(&htim4);

    // Calculate pulse width based on input percentage (e.g., 60% of period)
    uint32_t pulse = (uint32_t)((period + 1) * (speed_percent / 100.0f));

    if (speed_percent == 0) {
        // STOP state: Set both direction pins to LOW
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_RESET);
    } else {
        // RUNNING state: Forward direction (Set PC1 High, PC2 Low)
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_RESET);
    }

    // Update the Compare Register (CCR) to change the PWM duty cycle
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, pulse);
}
