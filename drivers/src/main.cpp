#include "motor.hpp"
#include "sensor.hpp"
#include "sht20.hpp"
#include "system_monitor.hpp"
#include "communication.hpp"
#include "audio.hpp"
#include "display.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace std;

// STM32와 연결된 UART fd. sensor.cpp, sht20.cpp, audio.cpp, display.cpp에서 공유 사용
int g_uart_fd = -1;

int main() {
    cout << "========================================" << endl;
    cout << "   Motor Pi - Start" << endl;
    cout << "========================================" << endl;

    // 1. MQTT 초기화 (communication.cpp)
    init_comm_mqtt();

    // 2. UART 초기화 및 CO/CO2 센서 수신 스레드 시작
    g_uart_fd = init_comm_uart("/dev/ttyS0");
    if (g_uart_fd >= 0) {
        thread([]() { receive_sensor_data(g_uart_fd); }).detach();
    } else {
        cerr << "❌ UART 초기화 실패 - 센서 수신 불가" << endl;
    }

    // 3. SHT20 온습도 센서 초기화 및 모니터링 스레드 시작
    int sht20_fd = init_sht20();
    if (sht20_fd >= 0) {
        thread([sht20_fd]() { run_sht20_monitor(sht20_fd); }).detach();
        cout << "📡 SHT20 온습도 모니터링 시작" << endl;
    } else {
        cerr << "❌ SHT20 초기화 실패" << endl;
    }

    // 4. 오디오 초기화 (STM32에서 WAV 목록 수신)
    if (g_uart_fd >= 0) {
        init_audio(g_uart_fd);
    }

    // 5. 디스플레이 MQTT 구독 초기화
    if (g_uart_fd >= 0) {
        init_display(g_uart_fd);
    }

    // 6. 시스템 상태 모니터링 스레드 시작 (5초 주기)
    thread([]() {
        while (true) {
            DeviceStats stats = get_server_stats();
            json payload = {
                {"device",     "firmware"},
                {"cpu_usage",  stats.cpu_usage},
                {"cpu_temp",   stats.cpu_temp},
                {"disk_usage", stats.disk_usage}
            };
            publish_to_server("system/firmware", payload.dump());
            cout << "📊 시스템 상태 전송: " << payload.dump() << endl;
            this_thread::sleep_for(chrono::seconds(5));
        }
    }).detach();

    cout << "✅ 모든 서비스 가동 중" << endl;

    while (true) {
        this_thread::sleep_for(chrono::hours(24));
    }

    close_uart(g_uart_fd);
    close_sht20(sht20_fd);
    return 0;
}