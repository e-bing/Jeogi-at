#include "motor.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include <mqtt/async_client.h>

#include "../includes/shared_data.hpp"

using json = nlohmann::json;
using namespace std;

bool g_auto_mode = true;

static const string MQTT_BROKER = g_mqtt_broker;
static const string MQTT_TOPIC  = "motor/control";
static const string CLIENT_ID   = "server_motor_pub";

static mqtt::async_client g_mqtt_client(MQTT_BROKER, CLIENT_ID);
static bool g_mqtt_connected = false;

void init_mqtt_motor() {
    try {
        mqtt::connect_options opts;
        opts.set_keep_alive_interval(20);
        opts.set_clean_session(true);
        opts.set_automatic_reconnect(true);

        g_mqtt_client.connect(opts)->wait();
        g_mqtt_connected = true;
        cout << "✅ MQTT 모터 브로커 연결 완료" << endl;
    } catch (const mqtt::exception& e) {
        cerr << "❌ MQTT 연결 실패: " << e.what() << endl;
    }
}

void send_motor_command(const string& action, int speed) {
    if (!g_mqtt_connected) {
        cerr << "❌ MQTT 미연결 - 모터 명령 전송 불가" << endl;
        return;
    }
    try {
        json cmd = {
            {"type", "motor_control"},
            {"action", action},
            {"speed", speed}
        };
        string payload = cmd.dump();
        g_mqtt_client.publish(MQTT_TOPIC, payload, 1, false)->wait();
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
            {"type", "mode_control"},
            {"action", mode}
        };
        string payload = cmd.dump();
        g_mqtt_client.publish(MQTT_TOPIC, payload, 1, false)->wait();
        cout << "✅ MQTT 모드 명령 전송: " << payload << endl;
    } catch (const mqtt::exception& e) {
        cerr << "❌ MQTT 모드 명령 전송 실패: " << e.what() << endl;
    }
}
