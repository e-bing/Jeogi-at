#include "display.hpp"
#include "communication.hpp"
#include "config_loader.h"
#include "../../protocol/message_types.hpp"
#include <chrono>
#include <climits>
#include <iostream>
#include <cstdio>
#include <mqtt/async_client.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>

using json = nlohmann::json;
using namespace std;

static const string DISPLAY_CLIENT_ID = "motor_pi_display_sub";

static mqtt::async_client *g_display_mqtt = nullptr;
static bool g_subway_thread_started = false;
static constexpr int TARGET_SUBWAY_ID = 1003; // 3호선

static bool fetch_subway_json_via_curl(const string &url, string &out_json) {
  out_json.clear();
  string cmd = "curl -s --max-time 5 \"" + url + "\"";
  FILE *fp = popen(cmd.c_str(), "r");
  if (!fp) return false;

  char buf[1024];
  while (fgets(buf, sizeof(buf), fp) != nullptr) {
    out_json += buf;
  }

  int rc = pclose(fp);
  return rc == 0 && !out_json.empty();
}

static bool parse_best_up_train(const string &json_text, int &out_barvl_dt, string &out_line, string &out_msg) {
  out_barvl_dt = -1;
  out_line.clear();
  out_msg.clear();

  try {
    auto root = json::parse(json_text);
    if (!root.contains("realtimeArrivalList") || !root["realtimeArrivalList"].is_array()) {
      return false;
    }

    int best = INT32_MAX;
    for (const auto &row : root["realtimeArrivalList"]) {
      string updn = row.value("updnLine", "");
      if (updn.find("상행") == string::npos) continue;
      int subway_id = row.value("subwayId", -1);
      if (subway_id != TARGET_SUBWAY_ID) continue;

      int sec = -1;
      if (row.contains("barvlDt")) {
        const auto &barvl = row["barvlDt"];
        if (barvl.is_string()) {
          try {
            sec = stoi(barvl.get<string>());
          } catch (...) {
            continue;
          }
        } else if (barvl.is_number_integer()) {
          sec = barvl.get<int>();
        } else {
          continue;
        }
      } else {
        continue;
      }
      if (sec < 0) continue;

      if (sec < best) {
        best = sec;
        out_line = row.value("trainLineNm", "");
        out_msg = row.value("arvlMsg2", "");
      }
    }

    if (best == INT32_MAX) return false;
    out_barvl_dt = best;
    return true;
  } catch (...) {
    return false;
  }
}

static void run_subway_arrival_override_loop(int uart_fd) {
  auto config = load_config();
  string api_key = config.value("subway_api_key", "");
  string station_encoded = config.value("subway_station_encoded", "%EC%96%91%EC%9E%AC");
  int poll_ms = config.value("subway_poll_ms", 15000);
  int hold_sec = config.value("subway_hold_sec", 5);
  int event_screen = config.value("subway_event_screen", 4);
  int base_screen = config.value("subway_base_screen", 0);

  if (api_key.empty()) {
    cerr << "[Subway] subway_api_key is empty; skipping subway API polling" << endl;
    return;
  }

  const string url = "http://swopenAPI.seoul.go.kr/api/subway/" + api_key +
                     "/json/realtimeStationArrival/0/5/" + station_encoded;

  bool active = false;
  auto active_until = chrono::steady_clock::now();
  int zero_streak = 0;

  cout << "[Subway] started: line=3(up) screen " << event_screen
       << " for " << hold_sec << "s on arrival" << endl;

  while (true) {
    auto now = chrono::steady_clock::now();
    if (active && now >= active_until) {
      send_to_stm32_display_screen(uart_fd, base_screen);
      cout << "[Subway] restore base screen=" << base_screen << endl;
      active = false;
    }

    string body;
    int barvl_dt = -1;
    string line;
    string msg;

    if (fetch_subway_json_via_curl(url, body) && parse_best_up_train(body, barvl_dt, line, msg)) {
      cout << "[Subway] up train: barvlDt=" << barvl_dt << " line=" << line << " msg=" << msg << endl;
      if (barvl_dt <= 0) {
        zero_streak++;
      } else {
        zero_streak = 0;
      }

      if (!active && zero_streak >= 2) {
        send_to_stm32_display_screen(uart_fd, event_screen);
        active = true;
        active_until = chrono::steady_clock::now() + chrono::seconds(hold_sec);
        zero_streak = 0;
        cout << "[Subway] arrival detected (2x confirm) -> event screen=" << event_screen << endl;
      }
    } else {
      zero_streak = 0;
    }

    this_thread::sleep_for(chrono::milliseconds(poll_ms));
  }
}

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

    if (!g_subway_thread_started) {
      g_subway_thread_started = true;
      thread([uart_fd]() { run_subway_arrival_override_loop(uart_fd); }).detach();
    }
  } catch (const mqtt::exception &e) {
    cerr << "[Display] MQTT 초기화 실패: " << e.what() << endl;
  }
}
