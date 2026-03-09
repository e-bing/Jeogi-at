#include "sht20.hpp"
#include "communication.hpp"
#include "../../protocol/message_types.hpp"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include <cmath>

using json = nlohmann::json;
using namespace std;

extern int g_uart_fd;

/**
 * @brief SHT20 디바이스 초기화
 * @return 성공 시 파일 디스크립터, 실패 시 -1
 */
int init_sht20() {
    int fd = open("/dev/sht20", O_RDONLY);
    if (fd < 0) {
        cerr << "❌ /dev/sht20 열기 실패. 드라이버를 확인하세요." << endl;
        return -1;
    }
    return fd;
}

/**
 * @brief 드라이버 문자열 파싱
 * 형식: "Temp: 25.40 C, Humi: 45.20 %"
 */
static bool parse_sht20_string(const char* buf, SHT20Data& data) {
    return sscanf(buf, "Temp: %f C, Humi: %f %%", &data.temperature, &data.humidity) == 2;
}

/**
 * @brief 실시간 온습도 모니터링 루프
 *
 * 2초마다 /dev/sht20 드라이버를 읽어:
 *   1. MQTT로 서버에 발행  (sensor/temp_humi)
 *   2. UART로 STM32에 바이너리 패킷 전송  (CMD 0x03)
 *
 * @param fd /dev/sht20 파일 디스크립터
 */
void run_sht20_monitor(int fd) {
    if (fd < 0) return;

    char buf[128];
    SHT20Data data;

    cout << "🚀 SHT20 모니터링 시작..." << endl;

    while (true) {
        lseek(fd, 0, SEEK_SET);
        ssize_t bytes = read(fd, buf, sizeof(buf) - 1);

        if (bytes > 0) {
            buf[bytes] = '\0';
            if (parse_sht20_string(buf, data)) {
                // 1. MQTT로 서버에 전송 (sensor/temp_humi)
                json payload = {
                    {Protocol::FIELD_TEMPERATURE, round(data.temperature * 100) / 100.0},
                    {Protocol::FIELD_HUMIDITY,    round(data.humidity    * 100) / 100.0}
                };
                publish_to_server(Protocol::MQTT_TOPIC_TEMP_HUMI, payload.dump());
                cout << "📤 [SHT20→MQTT] " << payload.dump() << endl;

                // 2. UART로 STM32에 전송 (CMD 0x03)
                send_to_stm32_temp_humi(g_uart_fd, data.temperature, data.humidity);
            }
        } else {
            cerr << "⚠️ SHT20 온습도 센서 읽기 실패" << endl;
        }

        this_thread::sleep_for(chrono::seconds(2));
    }
}

/**
 * @brief SHT20 종료 처리
 */
void close_sht20(int fd) {
    if (fd >= 0) close(fd);
}
