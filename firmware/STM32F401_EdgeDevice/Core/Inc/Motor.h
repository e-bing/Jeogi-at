#ifndef __MOTOR_H
#define __MOTOR_H

#include "main.h"

/* Function Prototypes */

/**
 * @brief Initializes the motor hardware and PWM settings.
 */
void Motor_Init(void);

/**
 * @brief Sets the motor speed based on percentage.
 * @param speed_percent: Speed value (0 to 100).
 */
void Motor_SetSpeed(uint8_t speed_percent);

#endif /* __MOTOR_H */
