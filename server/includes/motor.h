#ifndef MOTOR_H
#define MOTOR_H

#include <string>

extern bool g_auto_mode;

// 모터 명령 전송 함수
void send_motor_command(const std::string& action, int speed);

#endif // MOTOR_H
