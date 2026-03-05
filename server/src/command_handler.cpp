// command_handler.cpp
#include "../includes/command_handler.hpp"

using json = nlohmann::json;
using namespace std;

// Qt 명령 처리 함수 (Motor.h의 함수 호출)
void handle_qt_command(const string& cmd_str) {
  try {
    json data = json::parse(cmd_str);
    string type = data.value("type", "");

    if (type == "device_command") {
      json cmdData = data.value("data", json::object());
      string device = cmdData.value("device", "");
      string action = cmdData.value("action", "");

      // 1️⃣ 모드 제어 (자동/수동 전환)
      if (device == "mode_control") {
        if (action == "auto") {
          g_auto_mode = true;
          cout << "🤖 [MODE] 자동 모드 활성화 (센서 기반 제어)" << endl;
          send_mode_command("auto");
        } else if (action == "manual") {
          g_auto_mode = false;
          cout << "👤 [MODE] 수동 모드 활성화 (Qt 제어)" << endl;
          send_mode_command("manual");
        }
        return;
      }

      // 2️⃣ 장치 제어 (수동 모드일 때만 동작)
      if (!g_auto_mode) {
        if (device == "motor") {
          int speed = cmdData.value("speed", 100);

          if (action == "start" || action == "on") {
            cout << "🚀 [STATUS] MOTOR ON (Speed: " << speed << "%)" << endl;
            send_motor_command("start", speed);
          } else if (action == "stop" || action == "off") {
            cout << "🛑 [STATUS] MOTOR OFF" << endl;
            send_motor_command("stop", 0);
          }
        } else if (device == "speaker") {
          cout << "🔊 [STATUS] SPEAKER " << (action == "on" ? "ON" : "OFF")
               << endl;
        } else if (device == "lighting") {
          cout << "💡 [STATUS] LIGHTING " << (action == "on" ? "ON" : "OFF")
               << endl;
        }
      } else {
        cout << "⚠️ [AUTO MODE] Qt 수동 명령 무시됨 (현재 자동 모드 활성화 중)"
             << endl;
      }
    }  // if (type == "device_command") 닫기
  } catch (json::exception& e) {
    cerr << "❌ Qt 명령 파싱 에러: " << e.what() << endl;
  }
}