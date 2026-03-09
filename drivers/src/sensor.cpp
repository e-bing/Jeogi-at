#include "sensor.hpp"
#include "motor.hpp"
#include "communication.hpp"
#include <iostream>
#include <unistd.h>
#include <vector>
#include <chrono>
#include <thread>

using namespace std;

extern int g_uart_fd;

/**
 * @brief UART를 통해 STM32로부터 CO/CO2 센서 데이터를 수집하고 서버로 전송합니다.
 *
 * 주기: 약 1.2초 (GET_CO 300ms + sleep 100ms + GET_CO2 300ms + sleep 500ms)
 * communication.cpp의 read_packet, send_to_server_sensor를 사용합니다.
 * 자동 모드(g_auto_mode)일 때는 센서값에 따라 모터도 자동 제어합니다.
 *
 * @param uart_fd UART 파일 디스크립터
 */
void receive_sensor_data(int uart_fd) {
    if (uart_fd < 0) {
        cerr << "❌ [Sensor] UART 포트가 유효하지 않습니다." << endl;
        return;
    }

    cout << "🚀 센서 데이터 수집 시작 (1초 주기)" << endl;

    while (true) {
        float current_co  = 0.0f;
        float current_co2 = 0.0f;

        // 1. CO 요청
        send_to_stm32_get_co(uart_fd, current_co);

        this_thread::sleep_for(chrono::milliseconds(100));

        // 2. CO2 요청
        send_to_stm32_get_co2(uart_fd, current_co2);

        // 3. 서버로 CO/CO2 전송
        send_to_server_sensor(current_co, current_co2);

        // 4. 자동 모드일 때만 센서값으로 모터 제어
        if (g_auto_mode) {
            auto_motor_control(current_co2, current_co);
        }

        this_thread::sleep_for(chrono::milliseconds(500));
    }
}

/**
 * @brief UART 연결을 종료합니다.
 */
void close_uart(int uart_fd) {
    if (uart_fd >= 0) {
        close(uart_fd);
        cout << "UART 연결 종료" << endl;
    }
}