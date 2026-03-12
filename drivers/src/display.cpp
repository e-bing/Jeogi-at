#include "display.hpp"
#include "communication.hpp"
#include "config_loader.h"
#include "../../protocol/message_types.hpp"
#include <chrono>
#include <iostream>
#include <mqtt/async_client.h>
#include <nlohmann/json.hpp>
#include <vector>

using json = nlohmann::json;
using namespace std;

static const string DISPLAY_CLIENT_ID = "motor_pi_display_sub";

static mqtt::async_client *g_display_mqtt = nullptr;

/**
 * @brief MQTT 수신 콜백.
 *        서버에서 온 congestion_topic 메시지를 파싱하여 STM32로 일괄 혼잡도
 * 패킷을 전송합니다. 수신 JSON 형식: {"update_time": unix_sec, "levels":
 * [0,1,2,...], "zone_count": N} levels 원소: 0=여유, 1=보통, 2=혼잡 (최대 8개)
 */
class DisplayCallback : public mqtt::callback {
public:
  explicit DisplayCallback(int uart_fd) : uart_fd_(uart_fd) {}

  void message_arrived(mqtt::const_message_ptr msg) override {
    try {
      json data = json::parse(msg->get_payload_str());

      // display/control: {"type": "display_control", "action": "on"/"off"/...}
      if (data.value(Protocol::FIELD_TYPE, "") == Protocol::MSG_DISPLAY_CONTROL) {
        string action = data.value(Protocol::FIELD_ACTION, "");
        if (action.empty()) {
          cerr << "[Display] JSON 오류: action 누락" << endl;
          return;
        }
        cout << "[Display] 제어 명령 수신: " << action << endl;
        send_to_stm32_display_control(uart_fd_, action);
        return;
      }

      // iot/server/status/congestion: {"levels": [...], "update_time": ...}
      if (!data.contains("levels") || !data["levels"].is_array()) {
        cerr << "[Display] JSON 오류: 알 수 없는 메시지" << endl;
        return;
      }

      vector<int> levels;
      for (const auto &v : data["levels"]) {
        if (!v.is_number_integer()) {
          cerr << "[Display] JSON 오류: levels 원소 타입 오류" << endl;
          return;
        }
        levels.push_back(v.get<int>());
      }

      if (data.contains("update_time")) {
        long long update_time = data["update_time"].get<long long>();
        auto now = chrono::system_clock::to_time_t(chrono::system_clock::now());
        long long lag = static_cast<long long>(now) - update_time;
        if (lag > 30) {
          cerr << "[Display] 지연 데이터 감지: " << lag << "초 전" << endl;
        }
      }

      send_to_stm32_bulk_congestion(uart_fd_, levels);
    } catch (const exception &e) {
      cerr << "[Display] JSON 파싱 실패: " << e.what() << endl;
    }
  }

  void connection_lost(const string &cause) override {
    cerr << "[Display] MQTT 연결 끊김: " << cause << endl;
  }

private:
  int uart_fd_;
};

static DisplayCallback *g_display_cb = nullptr;

/**
 * @brief 디스플레이 MQTT 구독을 초기화합니다.
 *        수신 JSON 형식: {"update_time": 1234567890, "levels": [0,1,2,...],
 * "zone_count": 8} levels 원소: 0=여유, 1=보통, 2=혼잡
 */
void init_display(int uart_fd) {
  try {
    auto config = load_config();
    string broker = config.value("mqtt_broker", "tcp://localhost:1883");
    string topic =
        config.value("congestion_topic", Protocol::MQTT_TOPIC_CONGESTION_STATUS);

    g_display_mqtt = new mqtt::async_client(broker, DISPLAY_CLIENT_ID);
    g_display_cb = new DisplayCallback(uart_fd);
    g_display_mqtt->set_callback(*g_display_cb);

    mqtt::connect_options opts;
    opts.set_keep_alive_interval(20);
    opts.set_clean_session(true);
    opts.set_automatic_reconnect(true);

    g_display_mqtt->connect(opts)->wait();
    g_display_mqtt->subscribe(topic, 1)->wait();
    g_display_mqtt->subscribe(Protocol::MQTT_TOPIC_DIGITAL_DISPLAY_CONTROL, 1)->wait();
    cout << "[Display] MQTT 연결 완료, 구독: " << topic
         << ", " << Protocol::MQTT_TOPIC_DIGITAL_DISPLAY_CONTROL << endl;
  } catch (const mqtt::exception &e) {
    cerr << "[Display] MQTT 초기화 실패: " << e.what() << endl;
  }
}
