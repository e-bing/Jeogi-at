#include "motor.h"
#include <iostream>
#include <fstream>

using namespace std;

bool g_auto_mode = true;

/**
 * @brief /dev/motor 디바이스에 속도를 씁니다.
 * @param speed 모터 속도 (0-100)
 */
void control_motor(int speed) {
    ofstream dev("/dev/motor");
    if (!dev.is_open()) {
        cerr << "❌ /dev/motor 열기 실패" << endl;
        return;
    }
    dev << speed;
    dev.close();
    cout << "✅ 모터 속도 설정: " << speed << "%" << endl;
}

/**
 * @brief CO2/CO 임계값 기반으로 모터를 자동 제어합니다.
 *        - CO2 > 600ppm 또는 CO > 25ppm : 100% (위험)
 *        - CO2 > 400ppm 또는 CO > 9ppm  : 60%  (경고)
 *        - 그 외                         : 0%   (정상)
 * @param co2 CO2 농도 (ppm)
 * @param co  CO 농도 (ppm)
 */
void auto_motor_control(float co2, float co) {
    if (co2 > 600.0f || co > 25.0f) {
        cout << "🔴 위험 - 모터 100%" << endl;
        control_motor(100);
    } else if (co2 > 400.0f || co > 9.0f) {
        cout << "🟡 경고 - 모터 60%" << endl;
        control_motor(60);
    } else {
        cout << "🟢 정상 - 모터 정지" << endl;
        control_motor(0);
    }
}

/**
 * @brief 서버로부터 수신한 MQTT 명령을 처리합니다.
 *        모드 전환 및 수동 모터 제어를 담당합니다.
 * @param type   명령 타입 ("mode_control" | "motor_control")
 * @param action 동작 ("auto" | "manual" | "start" | "stop")
 * @param speed  모터 속도 (0-100, motor_control 시 사용)
 */
void handle_mqtt_command(const string& type, const string& action, int speed) {
    if (type == "mode_control") {
        if (action == "auto") {
            g_auto_mode = true;
            cout << "🤖 자동 모드 전환" << endl;
        } else if (action == "manual") {
            g_auto_mode = false;
            control_motor(0); // 수동 전환 시 모터 정지
            cout << "👤 수동 모드 전환" << endl;
        }
    }
    else if (type == "motor_control") {
        if (!g_auto_mode) {
            if (action == "start" || action == "on") {
                control_motor(speed);
            } else if (action == "stop" || action == "off") {
                control_motor(0);
            }
        } else {
            cout << "⚠️ 자동 모드 중 - 수동 명령 무시" << endl;
        }
    }
}
