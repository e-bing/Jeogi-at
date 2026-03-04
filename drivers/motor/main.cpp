#include "motor.h"
#include "sensor.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>
#include <mqtt/async_client.h>

using json = nlohmann::json;
using namespace std;

static const string BROKER    = "tcp://192.168.0.53:1883";
static const string CLIENT_ID = "motor_pi_sub";

/**
 * @brief 서버로부터 모터/모드 제어 명령을 수신하는 MQTT 콜백입니다.
 */
class MqttCallback : public mqtt::callback {
public:
    void message_arrived(mqtt::const_message_ptr msg) override {
        try {
            json data = json::parse(msg->get_payload_str());
            string type   = data.value("type", "");
            string action = data.value("action", "");
            int    speed  = data.value("speed", 0);
            handle_mqtt_command(type, action, speed);
        } catch (json::exception& e) {
            cerr << "❌ JSON 파싱 에러: " << e.what() << endl;
        }
    }

    void connection_lost(const string& cause) override {
        cerr << "⚠️ MQTT 연결 끊김: " << cause << endl;
    }
};

int main() {
    cout << "========================================" << endl;
    cout << "   Motor Pi - Start" << endl;
    cout << "========================================" << endl;

    // 1. UART 초기화 및 센서 수신 스레드 시작
    g_uart_fd = init_uart("/dev/ttyS0");
    if (g_uart_fd >= 0) {
        thread([]() { receive_sensor_data(g_uart_fd); }).detach();
    } else {
        cerr << "❌ UART 초기화 실패 - 센서 수신 불가" << endl;
    }

    // 2. MQTT 연결 및 명령 구독
    mqtt::async_client client(BROKER, CLIENT_ID);
    MqttCallback cb;
    client.set_callback(cb);

    mqtt::connect_options opts;
    opts.set_keep_alive_interval(20);
    opts.set_clean_session(true);
    opts.set_automatic_reconnect(true);

    try {
        client.connect(opts)->wait();
        cout << "✅ MQTT 브로커 연결 완료" << endl;

        client.subscribe("motor/control", 1)->wait();
        cout << "📡 구독 시작: motor/control" << endl;

        while (true) {
            this_thread::sleep_for(chrono::seconds(1));
        }
    } catch (const mqtt::exception& e) {
        cerr << "❌ MQTT 에러: " << e.what() << endl;
        return 1;
    }

    close_uart(g_uart_fd);
    return 0;
}
