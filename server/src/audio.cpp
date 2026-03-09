#include "audio.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <mqtt/async_client.h>

#include "../../protocol/message_types.hpp"
#include "../includes/shared_data.hpp"

using json = nlohmann::json;
using namespace std;

static const string MQTT_BROKER = g_mqtt_broker;
static const string MQTT_TOPIC = Protocol::MQTT_TOPIC_AUDIO_CONTROL;
static const string CLIENT_ID = "server_audio_pub";

static mqtt::async_client* g_mqtt_client = nullptr;
static bool g_mqtt_connected = false;

void init_mqtt_audio() {
  if (g_mqtt_connected && g_mqtt_client != nullptr) return;

  try {
    if (g_mqtt_client == nullptr) {
      g_mqtt_client = new mqtt::async_client(g_mqtt_broker, CLIENT_ID);
    }

    mqtt::connect_options opts;
    opts.set_keep_alive_interval(20);
    opts.set_clean_session(true);
    opts.set_automatic_reconnect(true);
    opts.set_connect_timeout(10);

    g_mqtt_client->connect(opts)->wait();

    g_mqtt_connected = true;
    cout << "✅ MQTT 스피커 브로커 연결 완료" << endl;
  } catch (const mqtt::exception& e) {
    g_mqtt_connected = false;
    cerr << "❌ MQTT 연결 실패 (스피커): " << e.what() << endl;
    cerr << "   브로커: " << g_mqtt_broker << endl;
    cerr << "   에러코드: " << e.get_reason_code() << endl;
  }
}

void send_audio_command(const string& action) {
  if (!g_mqtt_connected) {
    cerr << "❌ MQTT 미연결 - 오디오 명령 전송 불가" << endl;
    return;
  }

  try {
    json cmd = {
        {Protocol::FIELD_TYPE, Protocol::MSG_AUDIO_CONTROL},
        {Protocol::FIELD_FILENAME, action},
    };
    string payload = cmd.dump();

    // audio/control 토픽만 발행합니다.
    g_mqtt_client->publish(Protocol::MQTT_TOPIC_AUDIO_CONTROL, payload, 1, false)->wait();

    cout << "✅ MQTT 오디오 명령 전송: " << payload << endl;
  } catch (const mqtt::exception& e) {
    cerr << "❌ MQTT 오디오 명령 전송 실패: " << e.what() << endl;
  }
}
