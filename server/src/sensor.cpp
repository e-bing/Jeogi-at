// sensor.cpp
#include "sensor.hpp"

#include <mqtt/async_client.h>

#include <chrono>
#include <csignal>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <thread>

#include "../../protocol/message_types.hpp"
#include "database.hpp"

using json = nlohmann::json;
using namespace std;

// shared_data.hpp에 정의된 전역 변수 참조
extern string g_mqtt_broker;
extern volatile sig_atomic_t stop_flag;

static const string CLIENT_ID = "server_sensor_sub";

/* ─────────────────────────────────────────
   온습도 캐시
   sensor/temp_humi 수신 시 업데이트
   sensor/air_quality 수신 시 CO/CO2와 함께 DB 저장에 사용
───────────────────────────────────────── */
static float g_last_temp = 0.0f;
static float g_last_humi = 0.0f;
static bool g_temp_humi_valid = false;
static mutex g_temp_humi_mutex;

/**
 * @brief 캐시된 온습도 값을 반환합니다.
 * @param temp 온도 저장 참조 (°C)
 * @param humi 습도 저장 참조 (%)
 * @return 수신된 데이터가 있으면 true
 */
bool get_last_temp_humi(float& temp, float& humi) {
  lock_guard<mutex> lock(g_temp_humi_mutex);
  temp = g_last_temp;
  humi = g_last_humi;
  return g_temp_humi_valid;
}

/**
 * @brief MQTT로 펌웨어의 센서 데이터를 수신해 DB에 저장합니다.
 *
 * 구독 토픽:
 *   - sensor/air_quality : CO, CO2 수신 → 온습도 캐시와 함께 air_stats DB 저장
 *   - sensor/temp_humi   : 온도, 습도 수신 → 캐시만 업데이트
 *
 * MQTT 연결이 끊기면 5초 후 자동 재연결합니다.
 * stop_flag가 설정되면 루프를 종료합니다.
 *
 * @param conn DB 연결 핸들
 */
void receive_sensor_data(MYSQL* conn) {
  /* ─── MQTT 콜백 클래스 정의 ─── */
  class SensorCallback : public mqtt::callback {
    MYSQL* conn_;

   public:
    SensorCallback(MYSQL* c) : conn_(c) {}

    void message_arrived(mqtt::const_message_ptr msg) override {
      static auto last_print = chrono::steady_clock::now();

      try {
        string topic = msg->get_topic();
        json data = json::parse(msg->get_payload_str());

        // 현재 캐시된(가장 최신) 온습도 값을 가져온다.
        float current_temp, current_humi;
        get_last_temp_humi(current_temp, current_humi);

        /* CO, CO2 수신 시 통합 저장 */
        if (topic == Protocol::MQTT_TOPIC_AIR_QUALITY) {
          float co = data.value(Protocol::FIELD_CO, 0.0f);
          float co2 = data.value(Protocol::FIELD_CO2, 0.0f);

          // DB 통합 저장 수행
          if (save_sensor_data(conn_, co, co2, current_temp, current_humi)) {
            auto now = chrono::steady_clock::now();
            if (chrono::duration_cast<chrono::seconds>(now - last_print)
                    .count() >= 10) {
              last_print = now;
              cout << "[SENSOR] CO:" << co << " CO2:" << co2
                   << " T:" << current_temp << "°C H:" << current_humi << "%"
                   << endl;
            }
          }
        }

        /* 온습도 수신 시 캐시만 업데이트 */
        else if (topic == Protocol::MQTT_TOPIC_TEMP_HUMI) {
          float temp = data.value(Protocol::FIELD_TEMPERATURE, 0.0f);
          float humi = data.value(Protocol::FIELD_HUMIDITY, 0.0f);

          // 캐시 업데이트 (Qt 전송 및 다음 DB 저장 시 사용)
          {
            lock_guard<mutex> lock(g_temp_humi_mutex);
            g_last_temp = temp;
            g_last_humi = humi;
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
      client.subscribe(Protocol::MQTT_TOPIC_AIR_QUALITY, 1)->wait();
      cout << "📡 구독 시작: sensor/air_quality" << endl;

      client.subscribe(Protocol::MQTT_TOPIC_TEMP_HUMI, 1)->wait();
      cout << "📡 구독 시작: sensor/temp_humi" << endl;

      while (client.is_connected() && !stop_flag) {
        this_thread::sleep_for(chrono::seconds(1));
      }

      try {
        client.disconnect()->wait();
      } catch (...) {
      }
      if (stop_flag) break;
      cerr << "⚠️ 센서 MQTT 연결 끊김 감지, 5초 후 재연결..." << endl;

    } catch (const mqtt::exception& e) {
      cerr << "❌ 센서 MQTT 에러: " << e.what() << " | 5초 후 재연결..."
           << endl;
    } catch (...) {
      cerr << "❌ 센서 스레드 알 수 없는 예외, 5초 후 재연결..." << endl;
    }
    if (!stop_flag) this_thread::sleep_for(chrono::seconds(5));
  }
}
