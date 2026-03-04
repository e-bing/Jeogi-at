#ifndef MOTOR_HPP
#define MOTOR_HPP

#include <string>

extern bool g_auto_mode;

void init_mqtt_motor();
void send_mode_command(const std::string& mode);
// 모터 명령 전송 함수
void send_motor_command(const std::string& action, int speed);

#endif // MOTOR_H
