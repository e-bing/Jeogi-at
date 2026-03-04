#include "sht20.h"
#include "sensor.h"
#include "config_loader.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include <mqtt/async_client.h>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cmath>

using json = nlohmann::json;
using namespace std;

// 전역 변수 설정
static mqtt::async_client* g_sht20_mqtt = nullptr;
static string g_sht20_topic;
static bool g_sht20_mqtt_connected = false;

// 프로토콜 상수
static constexpr uint8_t PKT_STX       = 0xAA;
static constexpr uint8_t PKT_ETX       = 0x55;
static constexpr uint8_t CMD_TEMP_HUMI = 0x03;  // 온습도 전송

/**
 * @brief CRC 계산 (cmd ^ len ^ ETX ^ data 전체 XOR)
 */
static uint8_t calc_crc(uint8_t cmd, uint8_t len, const uint8_t* data) {
    uint8_t crc = cmd ^ len;
    for (int i = 0; i < len; i++) crc ^= data[i];
    crc ^= PKT_ETX;
    return crc;
}

/**
 * @brief 온습도 데이터를 바이너리 프로토콜로 STM32에 전송합니다.
 *
 * 패킷 구조:
 *   [STX 0xAA][CMD 0x03][LEN 0x04][TEMP_H][TEMP_L][HUMI_H][HUMI_L][CRC][ETX 0x55]
 *
 *   온도/습도는 float * 100 후 int16_t 빅엔디안으로 전송합니다.
 *   예) 24.55°C → 2455 (0x09 0x97), 42.10% → 4210 (0x10 0x72)
 *
 * @param temp 온도 (°C)
 * @param humi 습도 (%)
 */
static void send_uart_temp_humi(float temp, float humi) {
    if (g_uart_fd < 0) return;

    int16_t temp_raw = static_cast<int16_t>(round(temp * 100));
    int16_t humi_raw = static_cast<int16_t>(round(humi * 100));

    uint8_t data[4] = {
        static_cast<uint8_t>((temp_raw >> 8) & 0xFF),  // 온도 상위 바이트
        static_cast<uint8_t>(temp_raw        & 0xFF),  // 온도 하위 바이트
        static_cast<uint8_t>((humi_raw >> 8) & 0xFF),  // 습도 상위 바이트
        static_cast<uint8_t>(humi_raw        & 0xFF)   // 습도 하위 바이트
    };

    uint8_t pkt[9] = {
        PKT_STX,
        CMD_TEMP_HUMI,
        0x04,
        data[0], data[1], data[2], data[3],
        calc_crc(CMD_TEMP_HUMI, 0x04, data),
        PKT_ETX
    };

    write(g_uart_fd, pkt, sizeof(pkt));
    cout << "📤 [UART→STM32] Temp: " << temp << "°C, Humi: " << humi << "%" << endl;
}

/**
 * @brief MQTT 연결 초기화 함수
 * config.json에서 브로커 주소와 토픽명을 읽어와 객체를 생성한다.
 */
static void init_sht20_mqtt() {
    try {
        auto config = load_config();
        string broker = config.value("mqtt_broker", "tcp://192.168.0.53:1883");
        g_sht20_topic = config.value("sht20_topic", "sensor/temp_humi");

        // 동적 할당을 통해 런타임에 브로커 주소를 적용한다.
        g_sht20_mqtt = new mqtt::async_client(broker, "motor_pi_sht20_pub");

        mqtt::connect_options opts;
        opts.set_keep_alive_interval(20);
        opts.set_clean_session(true);
        opts.set_automatic_reconnect(true);

        g_sht20_mqtt->connect(opts)->wait();
        g_sht20_mqtt_connected = true;
        cout << "✅ SHT20 MQTT 연결 완료: " << broker << " (Topic: " << g_sht20_topic << ")" << endl;
    } catch (const mqtt::exception& e) {
        cerr << "❌ SHT20 MQTT 연결 실패: " << e.what() << endl;
    }
}

/**
 * @brief SHT20 디바이스 초기화
 * @return 성공 시 파일 디스크립터, 실패 시 -1
 */
int init_sht20() {
    int fd = open("/dev/sht20", O_RDONLY);
    if (fd < 0) {
        cerr << "❌ /dev/sht20 열기 실패. 드라이버가 로드되었는지 확인하세요." << endl;
        return -1;
    }
    init_sht20_mqtt();
    return fd;
}

/**
 * @brief 드라이버에서 읽은 문자열을 파싱한다.
 * 형식: "Temp: 25.40 C, Humi: 45.20 %"
 */
static bool parse_sht20_string(const char* buf, SHT20Data& data) {
    if (sscanf(buf, "Temp: %f C, Humi: %f %%", &data.temperature, &data.humidity) == 2) {
        return true;
    }
    return false;
}

/**
 * @brief 실시간 온습도 모니터링 루프
 *
 * 2초마다 /dev/sht20 드라이버를 읽어:
 *   1. MQTT로 서버(boy)에 발행  (sensor/temp_humi)
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
        // 캐릭터 디바이스의 처음 위치부터 다시 읽는다.
        lseek(fd, 0, SEEK_SET);
        ssize_t bytes = read(fd, buf, sizeof(buf) - 1);
        
        if (bytes > 0) {
            buf[bytes] = '\0';
            if (parse_sht20_string(buf, data)) {

                // 1. MQTT 연결 상태 확인 후 데이터 전송 (서버로 전송)
                if (g_sht20_mqtt_connected && g_sht20_mqtt) {
                    try {
                        json payload = {
                            {"temperature", round(data.temperature * 100) / 100.0},
                            {"humidity", round(data.humidity * 100) / 100.0}
                        };
                        g_sht20_mqtt->publish(g_sht20_topic, payload.dump(), 1, false);
                        cout << "📤 [SHT20] 전송 완료 -> " << payload.dump() << endl;
                    } catch (const mqtt::exception& e) {
                        cerr << "❌ SHT20 데이터 전송 실패: " << e.what() << endl;
                    }
                }

                // 2. UART 전송 (STM32로 전송)
                send_uart_temp_humi(data.temperature, data.humidity);
            }
        } else {
            cerr << "⚠️ SHT20 드라이버 읽기 실패" << endl;
        }
        
        // 2초 대기
        this_thread::sleep_for(chrono::seconds(2));
    }
}

/**
 * @brief 종료 처리
 */
void close_sht20(int fd) {
    if (fd >= 0) {
        close(fd);
    }
    if (g_sht20_mqtt) {
        delete g_sht20_mqtt; // 할당된 메모리를 해제한다.
        g_sht20_mqtt = nullptr;
    }
}
