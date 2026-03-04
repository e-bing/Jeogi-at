#include "motor.h"
#include "sensor.h"
#include "system_monitor.h"
#include "sht20.h"
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

/**
 * @brief 5초마다 시스템 상태를 수집해 MQTT로 서버에 전송합니다.
 *        토픽: system/aboy
 *        전송 JSON: {"device":"aboy","cpu_usage":23.5,"cpu_temp":47.2,"disk_usage":61.3}
 * @param client MQTT 클라이언트 참조
 */
static void system_monitor_worker(mqtt::async_client& client) {
    while (true) {
        try {
            SystemStats stats = get_system_stats();

            json payload = {
                {"device",     "firmware"},
                {"cpu_usage",  stats.cpu_usage},
                {"cpu_temp",   stats.cpu_temp},
                {"disk_usage", stats.disk_usage}
            };

            client.publish("system/firmware", payload.dump(), 1, false)->wait();
            cout << "📊 시스템 상태 전송: " << payload.dump() << endl;

        } catch (const mqtt::exception& e) {
            cerr << "❌ 시스템 상태 전송 실패: " << e.what() << endl;
        }

        this_thread::sleep_for(chrono::seconds(5)); /* 5초마다 전송 */
    }
}

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

    // 2. SHT20 온습도 센서 초기화 및 스레드 시작 (추가된 부분)
    int sht20_fd = init_sht20();
    if (sht20_fd >= 0) {
        thread([sht20_fd]() { run_sht20_monitor(sht20_fd); }).detach();
        cout << "📡 SHT20 온습도 모니터링 시작" << endl;
    } else {
        cerr << "❌ SHT20 초기화 실패" << endl;
    }

    // 3. MQTT 연결 및 명령 구독
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

        // 3. 시스템 모니터링 스레드 시작
        thread([&client]() { system_monitor_worker(client); }).detach();
        cout << "📡 시스템 모니터링 시작 (5초 주기)" << endl;

        while (true) {
            this_thread::sleep_for(chrono::seconds(1));
        }

    } catch (const mqtt::exception& e) {
        cerr << "❌ MQTT 에러: " << e.what() << endl;
        while (true) {
		this_thread::sleep_for(chrono::seconds(1));
	}
    }

    close_uart(g_uart_fd);
    return 0;
}
