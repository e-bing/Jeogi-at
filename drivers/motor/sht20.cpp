#include "sht20.h"
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
 * 2초마다 드라이버를 읽고 JSON 형태로 MQTT 발행을 수행한다.
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
                // MQTT 연결 상태 확인 후 데이터 전송
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
