#include "audio.hpp"
#include "communication.hpp"
#include "config_loader.h"
#include "../../protocol/message_types.hpp"
#include <iostream>
#include <mqtt/async_client.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace std;

static int g_audio_uart_fd = -1;
static vector<string> g_wav_list;

static const string AUDIO_CLIENT_ID = "motor_pi_audio_sub";
static mqtt::async_client *g_audio_mqtt = nullptr;

class AudioCallback : public mqtt::callback {
public:
  void message_arrived(mqtt::const_message_ptr msg) override {
    try {
      json data = json::parse(msg->get_payload_str());

      string type = data.value(Protocol::FIELD_TYPE, "");
      if (type != Protocol::MSG_AUDIO_CONTROL) {
        cerr << "[Audio] 알 수 없는 메시지 타입: " << type << endl;
        return;
      }

      string filename = data.value(Protocol::FIELD_FILENAME, "");
      if (filename.empty()) {
        cerr << "[Audio] JSON 오류: filename 누락" << endl;
        return;
      }

      int index = stoi(filename) - 1;
      cout << "[Audio] 재생 명령 수신: " << filename << " → index " << index << endl;
      play_wav(index);
    } catch (const exception &e) {
      cerr << "[Audio] JSON 파싱 실패: " << e.what() << endl;
    }
  }

  void connection_lost(const string &cause) override {
    cerr << "[Audio] MQTT 연결 끊김: " << cause << endl;
  }
};

static AudioCallback *g_audio_cb = nullptr;

/**
 * @brief STM32로부터 WAV 파일 목록을 가져와 내부에 저장합니다.
 */
void init_audio(int uart_fd) {
  g_audio_uart_fd = uart_fd;

  g_wav_list = send_to_stm32_get_wavs(uart_fd);

  for (auto &f : g_wav_list)
    cout << " - " << f << "\n";

  // [테스트용] 초기화 직후 첫 번째 WAV 파일 즉시 재생
   if (!g_wav_list.empty()) play_wav(0);
}

/**
 * @brief 파일명으로 WAV를 재생합니다.
 */
void play_wav(const string &filename) {
  if (g_audio_uart_fd < 0) {
    cerr << "[Audio] UART 미초기화 - play_wav 불가" << endl;
    return;
  }
  send_to_stm32_play_wav(g_audio_uart_fd, filename);
}

/**
 * @brief 인덱스로 WAV를 재생합니다.
 */
void play_wav(int index) {
  if (index < 0 || index >= static_cast<int>(g_wav_list.size())) {
    cerr << "[Audio] 잘못된 인덱스: " << index << endl;
    return;
  }
  play_wav(g_wav_list[index]);
}

/**
 * @brief 저장된 WAV 파일 목록을 반환합니다.
 */
const vector<string> &get_wav_list() { return g_wav_list; }

/**
 * @brief 오디오 MQTT 구독을 초기화합니다.
 *        수신 JSON 형식: {"type": "audio_control", "filename": "1"}
 */
void init_audio_mqtt() {
  try {
    auto config = load_config();
    string broker = config.value("mqtt_broker", "tcp://localhost:1883");

    g_audio_mqtt = new mqtt::async_client(broker, AUDIO_CLIENT_ID);
    g_audio_cb = new AudioCallback();
    g_audio_mqtt->set_callback(*g_audio_cb);

    mqtt::connect_options opts;
    opts.set_keep_alive_interval(20);
    opts.set_clean_session(true);
    opts.set_automatic_reconnect(true);

    g_audio_mqtt->connect(opts)->wait();
    g_audio_mqtt->subscribe(Protocol::MQTT_TOPIC_AUDIO_CONTROL, 1)->wait();
    cout << "[Audio] MQTT 연결 완료, 구독: "
         << Protocol::MQTT_TOPIC_AUDIO_CONTROL << endl;
  } catch (const mqtt::exception &e) {
    cerr << "[Audio] MQTT 초기화 실패: " << e.what() << endl;
  }
}
