#include "motor.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <mqtt/async_client.h>

#include "../../protocol/message_types.hpp"
#include "../includes/shared_data.hpp"

using json = nlohmann::json;
using namespace std;

bool g_auto_mode = true;

static const string MQTT_BROKER = g_mqtt_broker;
static const string MQTT_TOPIC  = Protocol::MQTT_TOPIC_MOTOR_CONTROL;
static const string CLIENT_ID   = "server_motor_pub";

static mqtt::async_client* g_mqtt_client = nullptr; 
static bool g_mqtt_connected = false;

void init_mqtt_motor() {
    // 이미 연결되어 있다면 중복 실행 방지
    if (g_mqtt_connected && g_mqtt_client != nullptr) return;

    try {
        // 1. 실제 함수가 호출되는 시점에 객체를 생성 (이때 g_mqtt_broker 값을 읽음)
        if (g_mqtt_client == nullptr) {
            g_mqtt_client = new mqtt::async_client(g_mqtt_broker, CLIENT_ID);
        }

        mqtt::connect_options opts;
        opts.set_keep_alive_interval(20);
        opts.set_clean_session(true);
        opts.set_automatic_reconnect(true);
        opts.set_connect_timeout(10);

        // 2. 포인터를 사용하므로 . 대신 -> 연산자 사용
        g_mqtt_client->connect(opts)->wait();
        
        g_mqtt_connected = true;
        cout << "✅ MQTT 모터 브로커 연결 완료" << endl;
    } catch (const mqtt::exception& e) {
        g_mqtt_connected = false;
        cerr << "❌ MQTT 연결 실패: " << e.what() << endl;
        cerr << "   브로커: " << g_mqtt_broker << endl;
        cerr << "   에러코드: " << e.get_reason_code() << endl;
    }
}

void send_motor_command(const string& action, int speed) {
    if (!g_mqtt_connected) {
        cerr << "❌ MQTT 미연결 - 모터 명령 전송 불가" << endl;
        return;
    }
    try {
        json cmd = {
            {Protocol::FIELD_TYPE, Protocol::MSG_MOTOR_CONTROL},
            {Protocol::FIELD_ACTION, action},
            {Protocol::FIELD_SPEED, speed}
        };
        string payload = cmd.dump();
        g_mqtt_client->publish(MQTT_TOPIC, payload, 1, false)->wait();
        cout << "✅ MQTT 모터 명령 전송: " << payload << endl;
    } catch (const mqtt::exception& e) {
        cerr << "❌ MQTT 모터 명령 전송 실패: " << e.what() << endl;
    }
}

void send_mode_command(const string& mode) {
    if (!g_mqtt_connected) {
        cerr << "❌ MQTT 미연결 - 모드 명령 전송 불가" << endl;
        return;
    }
    try {
        json cmd = {
            {Protocol::FIELD_TYPE, Protocol::MSG_MODE_CONTROL},
            {Protocol::FIELD_ACTION, mode}
        };
        string payload = cmd.dump();
        g_mqtt_client->publish(MQTT_TOPIC, payload, 1, false)->wait();
        cout << "✅ MQTT 모드 명령 전송: " << payload << endl;
    } catch (const mqtt::exception& e) {
        cerr << "❌ MQTT 모드 명령 전송 실패: " << e.what() << endl;
    }
}
