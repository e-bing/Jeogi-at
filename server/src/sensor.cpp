// sensor.cpp
#include "sensor.h"
#include "database.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include <mqtt/async_client.h>

#include "../includes/shared_data.hpp"

using json = nlohmann::json;
using namespace std;

static const string BROKER = g_mqtt_broker;
static const string CLIENT_ID   = "server_sensor_sub";

/**
 * @brief aboy로부터 MQTT로 센서값을 수신해서 DB에 저장합니다.
 * @param conn DB 연결 핸들
 */
void receive_sensor_data(MYSQL* conn) {
    class SensorCallback : public mqtt::callback {
        MYSQL* conn_;
    public:
        SensorCallback(MYSQL* c) : conn_(c) {}

        void message_arrived(mqtt::const_message_ptr msg) override {
            try {
                json data = json::parse(msg->get_payload_str());
                float co  = data.value("co",  0.0f);
                float co2 = data.value("co2", 0.0f);
                cout << "📊 [센서 수신] CO: " << co << " ppm | CO2: " << co2 << " ppm" << endl;
                save_sensor_data(conn_, co, co2);
            } catch (json::exception& e) {
                cerr << "⚠️ JSON 에러: " << e.what() << endl;
            }
        }

        void connection_lost(const string& cause) override {
            cerr << "⚠️ 센서 MQTT 연결 끊김: " << cause << endl;
        }
    };

    try {
        mqtt::async_client client(BROKER, CLIENT_ID);
        SensorCallback cb(conn);
        client.set_callback(cb);

        mqtt::connect_options opts;
        opts.set_keep_alive_interval(20);
        opts.set_clean_session(true);
        opts.set_automatic_reconnect(true);

        client.connect(opts)->wait();
        cout << "✅ 센서 MQTT 구독 연결 완료" << endl;

        client.subscribe("sensor/data", 1)->wait();
        cout << "📡 센서 데이터 구독 시작: sensor/data" << endl;

        while (true) {
            this_thread::sleep_for(chrono::seconds(1));
        }
    } catch (const mqtt::exception& e) {
        cerr << "❌ 센서 MQTT 에러: " << e.what() << endl;
    }
}
