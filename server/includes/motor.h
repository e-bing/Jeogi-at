#ifndef MOTOR_H
#define MOTOR_H

#include <string>

extern bool g_auto_mode;

void init_mqtt_motor();
void publish_sensor_mqtt(float co, float co2);
void send_mode_command(const std::string& mode);
// 모터 명령 전송 함수
void send_motor_command(const std::string& action, int speed);

#endif // MOTOR_H
