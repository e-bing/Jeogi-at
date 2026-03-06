#include "motor.hpp"
#include "../../protocol/message_types.hpp"
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
        cerr << "❌ [Motor] /dev/motor 열기 실패" << endl;
        return;
    }
    dev << speed;
    cout << "✅ 모터 속도 설정: " << speed << "%" << endl;
}

/**
 * @brief 센서값 기반으로 모터를 자동 제어합니다.
 *        CO2 > 600 or CO > 25 → 100% / CO2 > 400 or CO > 9 → 60% / 정상 → 0%
 *        g_auto_mode가 false(수동 모드)이면 아무 동작도 하지 않습니다.
 * @param co2 CO2 농도 (ppm)
 * @param co  CO 농도 (ppm)
 */
void auto_motor_control(float co2, float co) {
    if (!g_auto_mode) return;

    if (co2 > 600.0f || co > 25.0f) {
        cout << "🔴 위험 수치 감지 - 모터 100% 가동" << endl;
        control_motor(100);
    } else if (co2 > 400.0f || co > 9.0f) {
        cout << "🟡 경고 수치 감지 - 모터 60% 가동" << endl;
        control_motor(60);
    } else {
        cout << "🟢 정상 - 모터 정지" << endl;
        control_motor(0);
    }
}

/**
 * @brief 서버로부터 수신한 MQTT 명령을 처리합니다.
 *        type=Protocol::MSG_MODE_CONTROL  → g_auto_mode 전환 (auto: 자동, manual: 수동)
 *        type=Protocol::MSG_MOTOR_CONTROL → 수동 모드일 때만 모터 start/stop 처리
 * @param type   명령 타입 (Protocol::MSG_MODE_CONTROL | Protocol::MSG_MOTOR_CONTROL)
 * @param action 동작 (Protocol::ACTION_AUTO | Protocol::ACTION_MANUAL | Protocol::ACTION_START | Protocol::ACTION_STOP)
 * @param speed  모터 속도 (0-100, motor_control 시 사용)
 */
void handle_mqtt_command(const string& type, const string& action, int speed) {
    if (type == Protocol::MSG_MODE_CONTROL) {
        if (action == Protocol::ACTION_AUTO) {
            g_auto_mode = true;
            cout << "🤖 모드 변경: 자동 모드" << endl;
        } else if (action == Protocol::ACTION_MANUAL) {
            g_auto_mode = false;
            cout << "👤 모드 변경: 수동 모드" << endl;
        }
    } else if (type == Protocol::MSG_MOTOR_CONTROL) {
        if (g_auto_mode) {
            cout << "⚠️ 자동 모드 중 - 수동 제어 불가" << endl;
            return;
        }
        if (action == Protocol::ACTION_START) {
            cout << "▶️ 모터 수동 시작 (" << speed << "%)" << endl;
            control_motor(speed);
        } else if (action == Protocol::ACTION_STOP) {
            cout << "⏹️ 모터 수동 정지" << endl;
            control_motor(0);
        }
    }
}
