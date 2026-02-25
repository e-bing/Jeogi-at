#ifndef MOTOR_H
#define MOTOR_H

#include <string>

extern bool g_auto_mode;

/**
 * @brief /dev/motor 디바이스에 속도를 씁니다.
 * @param speed 모터 속도 (0-100)
 */
void control_motor(int speed);

/**
 * @brief 센서값 기반으로 모터를 자동 제어합니다.
 * @param co2 CO2 농도 (ppm)
 * @param co  CO 농도 (ppm)
 */
void auto_motor_control(float co2, float co);

/**
 * @brief 서버로부터 수신한 MQTT 명령을 처리합니다.
 *        모드 전환 및 수동 모터 제어를 담당합니다.
 * @param type   명령 타입 ("mode_control" | "motor_control")
 * @param action 동작 ("auto" | "manual" | "start" | "stop")
 * @param speed  모터 속도 (0-100, motor_control 시 사용)
 */
void handle_mqtt_command(const std::string& type, const std::string& action, int speed);

#endif // MOTOR_H
