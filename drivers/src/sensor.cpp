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
        vector<uint8_t> rx_pkt;

        // 1. GET_CO 요청 전송 및 응답 수신 (300ms 대기)
        // 패킷: [AA][01][00][54][55]
        uint8_t req_co[] = {0xAA, 0x01, 0x00, 0x54, 0x55};
        write(uart_fd, req_co, sizeof(req_co));

        if (read_packet(uart_fd, rx_pkt, 300)) {
            // 응답 프레임: [STX][CMD][LEN][DATA...][CRC][ETX]
            // rx_pkt[3], rx_pkt[4] = CO 값 상위/하위 바이트, 단위: 0.01 ppm
            if (rx_pkt.size() >= 7) {
                current_co = (float)((rx_pkt[3] << 8) | rx_pkt[4]) / 100.0f;
            }
        }

        this_thread::sleep_for(chrono::milliseconds(100));

        // 2. GET_CO2 요청 전송 및 응답 수신 (300ms 대기)
        // 패킷: [AA][02][00][57][55]
        uint8_t req_co2[] = {0xAA, 0x02, 0x00, 0x57, 0x55};
        write(uart_fd, req_co2, sizeof(req_co2));

        rx_pkt.clear();
        if (read_packet(uart_fd, rx_pkt, 300)) {
            // rx_pkt[3], rx_pkt[4] = CO2 값 상위/하위 바이트, 단위: 0.01 ppm
            if (rx_pkt.size() >= 7) {
                current_co2 = (float)((rx_pkt[3] << 8) | rx_pkt[4]) / 100.0f;
            }
        }

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