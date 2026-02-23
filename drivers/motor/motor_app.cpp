#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>
#include <mqtt/async_client.h>

using json = nlohmann::json;
using namespace std;

const string BROKER    = "tcp://192.168.0.53:1883";
const string CLIENT_ID = "motor_pi_sub";

bool g_auto_mode = true;

void control_motor(int speed) {
    ofstream dev("/dev/motor");
    if (!dev.is_open()) {
        cerr << "❌ /dev/motor 열기 실패" << endl;
        return;
    }
    dev << speed;
    dev.close();
    cout << "✅ 모터 속도 설정: " << speed << "%" << endl;
}

void auto_motor_control(float co2, float co) {
    if (co2 > 600.0f || co > 25.0f) {
        cout << "🔴 위험 - 모터 100%" << endl;
        control_motor(100);
    } else if (co2 > 400.0f || co > 9.0f) {
        cout << "🟡 경고 - 모터 60%" << endl;
        control_motor(60);
    } else {
        cout << "🟢 정상 - 모터 정지" << endl;
        control_motor(0);
    }
}

class MqttCallback : public mqtt::callback {
public:
    void message_arrived(mqtt::const_message_ptr msg) override {
        string topic = msg->get_topic();
        try {
            json data = json::parse(msg->get_payload_str());

            if (topic == "motor/control") {
                string type = data.value("type", "");

                if (type == "mode_control") {
                    string action = data.value("action", "");
                    if (action == "auto") {
                        g_auto_mode = true;
                        cout << "🤖 자동 모드 전환" << endl;
                    } else if (action == "manual") {
                        g_auto_mode = false;
                        control_motor(0); // 수동 전환 시 일단 정지
                        cout << "👤 수동 모드 전환" << endl;
                    }
                }
                else if (type == "motor_control" && !g_auto_mode) {
                    string action = data.value("action", "");
                    int speed = data.value("speed", 0);
                    if (action == "start" || action == "on") {
                        control_motor(speed);
                    } else if (action == "stop" || action == "off") {
                        control_motor(0);
                    }
                }
                else if (type == "motor_control" && g_auto_mode) {
                    cout << "⚠️ 자동 모드 중 - 수동 명령 무시" << endl;
                }
            }

            else if (topic == "sensor/data" && g_auto_mode) {
                float co2 = data.value("co2", 0.0f);
                float co  = data.value("co",  0.0f);
                cout << "📊 센서값 수신 CO: " << co << " ppm | CO2: " << co2 << " ppm" << endl;
                auto_motor_control(co2, co);
            }

        } catch (json::exception& e) {
            cerr << "❌ JSON 파싱 에러: " << e.what() << endl;
        }
    }

    void connection_lost(const string& cause) override {
        cerr << "⚠️ MQTT 연결 끊김: " << cause << endl;
    }
};

int main() {
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
        client.subscribe("sensor/data",   1)->wait();
        cout << "📡 구독 시작: motor/control, sensor/data" << endl;

        while (true) {
            this_thread::sleep_for(chrono::seconds(1));
        }
    } catch (const mqtt::exception& e) {
        cerr << "❌ MQTT 에러: " << e.what() << endl;
        return 1;
    }
    return 0;
}
