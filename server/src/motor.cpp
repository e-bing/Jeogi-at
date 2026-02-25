#include "motor.h"
#include <iostream>
#include <unistd.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace std;

// Sensor.cpp에 정의된 전역 변수를 외부에서 참조
extern int g_uart_fd;

bool g_auto_mode = true; // auto or manual

void send_motor_command(const string& action, int speed) {
    if (g_uart_fd < 0) {
        cerr << "❌ UART 연결 없음 - 모터 명령 전송 불가" << endl;
        return;
    }

    // JSON 형식으로 명령 생성
    json cmd = {
        {"type", "motor_control"},
        {"action", action},
        {"speed", speed}
    };

    string cmd_str = cmd.dump() + "\n";
    
    // STM32로 전송
    int bytes = write(g_uart_fd, cmd_str.c_str(), cmd_str.length());
    if (bytes > 0) {
        cout << "✅ STM32로 모터 명령 전송: " << cmd_str;
    } else {
        cerr << "❌ STM32 모터 명령 전송 실패" << endl;
    }
}
