#ifndef __MOTOR_H
#define __MOTOR_H

#include "main.h"

// 모터 제어 상태 정의
typedef enum {
    MOTOR_STOP = 0,
    MOTOR_SPEED_50,
    MOTOR_SPEED_100
} MotorState_t;

// 함수 프로토타입
void Motor_Init(void);
void Motor_SetSpeed(uint8_t speed_percent);

#endif /* __MOTOR_H */
