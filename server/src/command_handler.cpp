// command_handler.cpp
#include "../includes/command_handler.hpp"

#include "../../protocol/message_types.hpp"

using json = nlohmann::json;
using namespace std;

// Qt 명령 처리 함수
void handle_qt_command(const string& cmd_str) {
  try {
    json data = json::parse(cmd_str);
    string type = data.value(Protocol::FIELD_TYPE, "");

    // 1. 디바이스 제어 명령
    if (type == Protocol::MSG_DEVICE_COMMAND) {
      json cmdData = data.value(Protocol::FIELD_DATA, json::object());
      string device = cmdData.value(Protocol::FIELD_DEVICE, "");
      string action = cmdData.value(Protocol::FIELD_ACTION, "");

      // 1️⃣ 모드 제어 (자동/수동 전환)
      if (device == Protocol::DEVICE_MODE_CONTROL) {
        if (action == Protocol::ACTION_AUTO) {
          g_auto_mode = true;
          cout << "🤖 [MODE] 자동 모드 활성화 (센서 기반 제어)" << endl;
          send_mode_command(Protocol::ACTION_AUTO);
        } else if (action == Protocol::ACTION_MANUAL) {
          g_auto_mode = false;
          cout << "👤 [MODE] 수동 모드 활성화 (Qt 제어)" << endl;
          send_mode_command(Protocol::ACTION_MANUAL);
        }
        return;
      }

      // 2️⃣ 장치 제어 (수동 모드일 때만 동작)
      if (!g_auto_mode) {
        if (device == Protocol::DEVICE_MOTOR) {
          int speed = cmdData.value(Protocol::FIELD_SPEED, 100);

          if (action == Protocol::ACTION_START ||
              action == Protocol::ACTION_ON) {
            cout << "🚀 [STATUS] MOTOR ON (Speed: " << speed << "%)" << endl;
            send_motor_command(Protocol::ACTION_START, speed);
          } else if (action == Protocol::ACTION_STOP ||
                     action == Protocol::ACTION_OFF) {
            cout << "🛑 [STATUS] MOTOR OFF" << endl;
            send_motor_command(Protocol::ACTION_STOP, 0);
          }
        } else if (device == Protocol::DEVICE_SPEAKER) {
          cout << "🔊 [STATUS] AUDIO command=" << action << endl;
          send_audio_command(action);
        } else if (device == Protocol::DEVICE_DIGITAL_DISPLAY) {
          cout << "🖥️ [STATUS] DIGITAL DISPLAY command=" << action << endl;
          send_display_command(action);
        }
      } else {
        cout << "⚠️ [AUTO MODE] Qt 수동 명령 무시됨 (현재 자동 모드 활성화 중)"
             << endl;
      }
    }
    // 2. ROI 업데이트 명령
    else if (type == Protocol::MSG_ROI_UPDATE) {
      json payload = data.value(Protocol::FIELD_DATA, json::object());

      string camera_id = payload.value("camera_id", "");
      int zone_id = payload.value("zone_id", -1);
      vector<float> roi = payload.value("roi", vector<float>());

      if (camera_id.empty() || zone_id == -1 || roi.size() != 4) {
        cerr << "[ERROR] 유효하지 않은 ROI 데이터 수신" << endl;
        return;
      }
      json config = ConfigManager::load();

      if (!config.contains("zones") || !config["zones"].is_array()) {
        config["zones"] = json::array();
      }
      json& zones = config["zones"];

      bool zone_updated = false;

      for (auto& zone : zones) {
        if (zone.value("zone_id", -1) == zone_id &&
            zone.value("camera_id", "") == camera_id) {
          zone["roi"] = roi;
          zone_updated = true;
          break;
        }
      }

      if (!zone_updated) {
        zones.push_back(
            {{"zone_id", zone_id}, {"camera_id", camera_id}, {"roi", roi}});
      }

      ConfigManager::save(config);
      cout << "[SUCCESS] config.json ROI 업데이트 완료 (Camera: " << camera_id
           << ", Zone: " << zone_id << ")" << endl;

      g_analyzer.loadConfig();
    }
  } catch (json::exception& e) {
    cerr << "❌ Qt 명령 파싱 에러: " << e.what() << endl;
  }
}