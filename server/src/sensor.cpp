// sensor.cpp
#include "sensor.h"
#include "database.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include <mqtt/async_client.h>
#include <csignal>

#include "../includes/shared_data.hpp"

using json = nlohmann::json;
using namespace std;

extern volatile sig_atomic_t stop_flag;

static const string CLIENT_ID = "server_sensor_sub";

/* ─────────────────────────────────────────
   온습도 캐시 로직
   MQTT 콜백에서 업데이트, Qt 전송 시 읽음
───────────────────────────────────────── */
static float g_last_temp = 0.0f;
static float g_last_humi = 0.0f;
static bool  g_temp_humi_valid = false;
static mutex g_temp_humi_mutex;

/**
 * @brief 마지막으로 수신한 온습도 값을 반환합니다.
 * @param temp 온도 저장 참조
 * @param humi 습도 저장 참조
 * @return 유효한 데이터가 있으면 true
 */
bool get_last_temp_humi(float& temp, float& humi)
{
    lock_guard<mutex> lock(g_temp_humi_mutex);
    temp = g_last_temp;
    humi = g_last_humi;
    return g_temp_humi_valid;
}

/**
 * @brief aboy로부터 MQTT로 센서값을 수신해서 DB에 저장합니다.
 * @param conn DB 연결 핸들
 */
void receive_sensor_data(MYSQL* conn) {

    /* ─── MQTT 콜백 클래스 정의 ─── */
    class SensorCallback : public mqtt::callback {
        MYSQL* conn_;
    public:
        SensorCallback(MYSQL* c) : conn_(c) {}

    void message_arrived(mqtt::const_message_ptr msg) override {
    try {
        string topic = msg->get_topic();
        json   data  = json::parse(msg->get_payload_str());

        // 현재 캐시된(가장 최신) 온습도 값을 가져온다.
        float current_temp, current_humi;
        get_last_temp_humi(current_temp, current_humi);

        /* sensor/air_quality → CO, CO2 수신 시 통합 저장 */
        if (topic == "sensor/air_quality") {
            float co  = data.value("co",  0.0f);
            float co2 = data.value("co2", 0.0f);
            
            // DB 통합 저장 수행
            if (save_sensor_data(conn_, co, co2, current_temp, current_humi)) {
                // 수신된 모든 값을 한눈에 확인할 수 있게 출력한다.
                cout << "----------------------------------------" << endl;
                cout << "📊 [SENSOR DATA RECEIVED]" << endl;
                cout << "   CO    : " << co << " ppm" << endl;
                cout << "   CO2   : " << co2 << " ppm" << endl;
                cout << "   TEMP  : " << current_temp << " °C" << endl;
                cout << "   HUMI  : " << current_humi << " %" << endl;
                cout << "----------------------------------------" << endl;
            }
        }

        /* sensor/temp_humi → 온습도 수신 시 캐시만 업데이트 */
        else if (topic == "sensor/temp_humi") {
            float temp = data.value("temperature", 0.0f);
            float humi = data.value("humidity",    0.0f);

            // 캐시 업데이트 로직
            {
                lock_guard<mutex> lock(g_temp_humi_mutex);
                g_last_temp       = temp;
                g_last_humi       = humi;
                g_temp_humi_valid = true;
            }
        }
    } catch (json::exception& e) {
        cerr << "⚠️ JSON 에러: " << e.what() << endl;
    }
}
        void connection_lost(const string& cause) override {
            cerr << "⚠️ 센서 MQTT 연결 끊김: " << cause << endl;
        }
    };

    // 바깥 while로 감싸서 재연결 + 예외가 터져도 스레드가 죽지 않음
    while (!stop_flag) {
        try {
            mqtt::async_client client(g_mqtt_broker, CLIENT_ID);
            SensorCallback cb(conn);
            client.set_callback(cb);

            mqtt::connect_options opts;
            opts.set_keep_alive_interval(20);
            opts.set_clean_session(true);
            opts.set_automatic_reconnect(true);

            client.connect(opts)->wait();
            cout << "✅ 센서 MQTT 구독 연결 완료" << endl;

            // CO/CO2 + 온습도 두 토픽 모두 구독
            client.subscribe("sensor/air_quality", 1)->wait();
            cout << "📡 구독 시작: sensor/air_quality" << endl;

            client.subscribe("sensor/temp_humi", 1)->wait();
            cout << "📡 구독 시작: sensor/temp_humi" << endl;

            // is_connected() 체크로 루프: 끊기면 바깥 while에서 재연결
            while (client.is_connected() && !stop_flag) {
                this_thread::sleep_for(chrono::seconds(1));
            }
            cerr << "⚠️ 센서 MQTT 연결 끊김 감지, 5초 후 재연결..." << endl;

        } catch (const mqtt::exception& e) {
            cerr << "❌ 센서 MQTT 에러: " << e.what() << " | 5초 후 재연결..." << endl;
        } catch (...) {
            cerr << "❌ 센서 스레드 알 수 없는 예외, 5초 후 재연결..." << endl;
        }
    if (!stop_flag)
        this_thread::sleep_for(chrono::seconds(5));
    }
}
