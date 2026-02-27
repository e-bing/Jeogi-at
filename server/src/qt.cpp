// qt.cpp
#include "../includes/qt.h"

using json = nlohmann::json;
using namespace std;

extern CongestionAnalyzer g_analyzer;
extern int g_uart_fd;
extern int get_total_people_count();

SSL_CTX* g_ssl_ctx = nullptr;
std::mutex g_ssl_send_mutex;

void init_tls() {
  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();

  g_ssl_ctx = SSL_CTX_new(TLS_server_method());
  if (!g_ssl_ctx) {
    cerr << "SSL_CTX_new 실패" << endl;
    ERR_print_errors_fp(stderr);
    exit(1);
  }

  if (SSL_CTX_use_certificate_file(g_ssl_ctx, "../config/server.crt",
                                   SSL_FILETYPE_PEM) <= 0) {
    cerr << "인증서 로드 실패: server.crt" << endl;
    ERR_print_errors_fp(stderr);
    exit(1);
  }

  if (SSL_CTX_use_PrivateKey_file(g_ssl_ctx, "../config/server.key",
                                  SSL_FILETYPE_PEM) <= 0) {
    cerr << "개인키 로드 실패: server.key" << endl;
    ERR_print_errors_fp(stderr);
    exit(1);
  }

  if (!SSL_CTX_check_private_key(g_ssl_ctx)) {
    cerr << "개인키와 인증서가 일치하지 않음" << endl;
    ERR_print_errors_fp(stderr);
    exit(1);
  }

  cout << "✅ TLS 초기화 완료 (TLS 1.2/1.3 지원)" << endl;
}

void cleanup_tls() {
  if (g_ssl_ctx) {
    SSL_CTX_free(g_ssl_ctx);
    g_ssl_ctx = nullptr;
  }
}

bool kill_process_using_port(int port) {
  string cmd = "lsof -ti:" + to_string(port) + " 2>/dev/null";
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) return true;

  char buffer[128];
  vector<string> pids;

  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    string pid = buffer;
    pid.erase(pid.find_last_not_of(" \n\r\t") + 1);
    if (!pid.empty()) {
      pids.push_back(pid);
    }
  }
  pclose(pipe);

  if (pids.empty()) return true;

  for (const auto& pid : pids) {
    string kill_cmd = "kill -9 " + pid + " 2>/dev/null";
    system(kill_cmd.c_str());
  }
  this_thread::sleep_for(chrono::milliseconds(500));
  return true;
}

// ✅ Qt 명령 처리 함수 (Motor.h의 함수 호출)
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

void handle_client(int client_socket) {
  SSL* ssl = SSL_new(g_ssl_ctx);
  if (!ssl) {
    cerr << "SSL_new 실패" << endl;
    close(client_socket);
    return;
  }

  SSL_set_fd(ssl, client_socket);

  if (SSL_accept(ssl) <= 0) {
    cerr << "TLS 핸드셰이크 실패" << endl;
    ERR_print_errors_fp(stderr);
    SSL_free(ssl);
    close(client_socket);
    return;
  }

  cout << "🔒 TLS 연결 성공 (Cipher: " << SSL_get_cipher(ssl) << ")" << endl;

  // Qt 클라이언트 전용 DB 연결
  DBConfig config;
  MYSQL* conn = connect_db(config);

  if (!conn) {
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_socket);
    return;
  }

  // 논블로킹 모드 설정 (명령 수신 대기 중 서버 멈춤 방지)
  int flags = fcntl(client_socket, F_GETFL, 0);
  fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);

  string cmd_buffer = "";

  bool client_connected = true;
  int db_tick = 0;

  // 영상 전송 전담 스레드 시작
  thread v_thread(video_streaming_worker, ssl, &client_connected);

  while (client_connected) {
    // ========== 1. Qt로부터 명령 수신 (논블로킹) ==========
    char rx_buffer[256];
    int bytes = SSL_read(ssl, rx_buffer, sizeof(rx_buffer) - 1);

    if (bytes > 0) {
      rx_buffer[bytes] = '\0';
      cmd_buffer += string(rx_buffer);

      size_t pos;
      while ((pos = cmd_buffer.find('\n')) != string::npos) {
        string cmd_line = cmd_buffer.substr(0, pos);
        cmd_buffer.erase(0, pos + 1);

        if (!cmd_line.empty() && cmd_line != "\r") {
          handle_qt_command(cmd_line);
        }
      }
    } else if (bytes < 0) {
      int err = SSL_get_error(ssl, bytes);
      if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) break;
    } else {
      // 에러 체크 및 연결 종료 감지
      int err = SSL_get_error(ssl, bytes);
      if (err != SSL_ERROR_WANT_READ) {
        client_connected = false;  // 스레드 종료 신호
        break;
      }
    }

    // [실시간 데이터 전송] - 매 루프마다 (약 10ms ~ 20ms 주기로)
    try {
      std::vector<int> zone_levels = g_analyzer.getCongestionLevels();
      json msg_zones = {{"type", "zone_congestion"},
                        {"zones", zone_levels},
                        {"total_count", get_total_people_count()}};
      string s_zones = msg_zones.dump() + "\n";

      lock_guard<mutex> lock(g_ssl_send_mutex);
      SSL_write(ssl, s_zones.c_str(), s_zones.length());
    } catch (...) {
    }

    // [DB 기반 데이터 전송] - db_tick이 100이 될 때마다 (약 1~2초 간격)
    if (++db_tick >= 100) {
      db_tick = 0;
      try {
        // 메시지들을 순차적으로 생성 및 전송
        json msg_realtime = {{"type", "realtime"},
                             {"title", "🚉 실시간 혼잡도"},
                             {"data", get_realtime_congestion(conn)}};
        json msg_air = {{"type", "realtime_air"},
                        {"title", "🌫️ 실시간 공기질"},
                        {"data", get_realtime_air_quality(conn)}};
        json msg_air_stats = {{"type", "air_stats"},
                              {"camera", "CAM-01"},
                              {"title", "📊 공기질 통계"},
                              {"data", get_air_quality_stats(conn, "CAM-01")}};
        json msg_flow = {{"type", "flow_stats"},
                         {"camera", "CAM-01"},
                         {"title", "👥 승객 흐름 통계"},
                         {"data", get_passenger_flow_stats(conn, "CAM-01")}};

        // 하나라도 전송 실패 시 루프 종료(disconnect)
        string s1 = msg_realtime.dump() + "\n";
        string s2 = msg_air.dump() + "\n";
        string s3 = msg_air_stats.dump() + "\n";
        string s4 = msg_flow.dump() + "\n";

        {
          lock_guard<mutex> lock(g_ssl_send_mutex);

          if (SSL_write(ssl, s1.c_str(), s1.length()) <= 0) break;
          if (SSL_write(ssl, s2.c_str(), s2.length()) <= 0) break;
          if (SSL_write(ssl, s3.c_str(), s3.length()) <= 0) break;
          if (SSL_write(ssl, s4.c_str(), s4.length()) <= 0) break;
        }

      } catch (const exception& e) {
        cerr << "데이터 전송 에러: " << e.what() << endl;
      }
    }

    this_thread::sleep_for(chrono::milliseconds(10));
  }

  client_connected = false;
  v_thread.join();
  SSL_shutdown(ssl);
  SSL_free(ssl);
  close_db(conn);
  close(client_socket);
  cout << "클라이언트 연결 종료" << endl;
}

bool send_camera_packet(SSL* ssl, uint32_t cam_id, const string& json_str,
                        const vector<unsigned char>& img_data) {
  CamProtocol::PacketHeader header;
  header.magic = htonl(CamProtocol::MAGIC_COOKIE);
  header.camera_id = htonl(cam_id);
  header.json_size = htonl(static_cast<uint32_t>(json_str.size()));
  header.image_size = htonl(static_cast<uint32_t>(img_data.size()));

  lock_guard<mutex> lock(g_ssl_send_mutex);  // SSL_write 보호

  if (SSL_write(ssl, &header, sizeof(header)) <= 0) return false;
  if (!json_str.empty())
    if (SSL_write(ssl, json_str.c_str(), json_str.size()) <= 0) return false;
  if (!img_data.empty())
    if (SSL_write(ssl, img_data.data(), img_data.size()) <= 0) return false;

  return true;
}

void video_streaming_worker(SSL* ssl, bool* client_connected) {
  while (*client_connected) {
    // 1. Hanwha 카메라 데이터 전송 (ID: 1, 15 FPS)
    {
      string json_payload;
      vector<unsigned char> jpg_buffer;
      {
        lock_guard<mutex> lock_frame(g_hw_frame_mutex);
        lock_guard<mutex> lock_data(g_hw_data_mutex);

        if (!g_hw_frame_buffer.empty()) {
          // [Step A] YUV Raw -> OpenCV Mat 변환
          int width = 1920;
          int height = 1080;
          cv::Mat yuv_frame(height * 1.5, width, CV_8UC1,
                            g_hw_frame_buffer.data());

          // [Step B] BGR 변환 및 리사이징 (성능을 위해 640x480 권장)
          cv::Mat bgr_frame, resized_frame;
          cv::cvtColor(yuv_frame, bgr_frame, cv::COLOR_YUV2BGR_I420);
          cv::resize(bgr_frame, resized_frame, cv::Size(640, 480));

          // [Step C] JPEG 압축 (압축률 80% 정도가 적당)
          cv::imencode(".jpg", resized_frame, jpg_buffer,
                       {cv::IMWRITE_JPEG_QUALITY, 80});

          // [Step D] JSON 생성
          json j;
          j["count"] = g_hw_objects.size();
          for (auto& o : g_hw_objects) {
            j["objs"].push_back(
                {{"x", o.x}, {"y", o.y}, {"w", o.w}, {"h", o.h}});
          }
          json_payload = j.dump();
        }
      }

      if (!jpg_buffer.empty()) {
        if (!send_camera_packet(ssl, 1, json_payload, jpg_buffer)) break;
      }
    }

    // 2. Pi Node 카메라들 (ID: 2~, 5 FPS)
    // 서브 카메라는 시간을 체크하여 200ms마다 전송
    static auto last_pi_send = chrono::steady_clock::now();
    if (chrono::steady_clock::now() - last_pi_send >=
        chrono::milliseconds(200)) {
      lock_guard<mutex> lock(g_node_map_mutex);
      uint32_t id_idx = 2;
      for (auto const& [id, camData] : g_pi_node_map) {
        string json_payload;
        vector<unsigned char> jpg_buffer;

        {
          lock_guard<mutex> d_lock(camData->data_mutex);
          if (!camData->frame_buffer.empty()) {
            // 1. Raw YUV -> JPEG 압축 (파이 노드용)
            // 파이 노드의 해상도(640x480 가정)에 맞춰 Mat 생성
            int w = 640;
            int h = 480;
            cv::Mat yuv_frame(h * 1.5, w, CV_8UC1,
                              camData->frame_buffer.data());
            cv::Mat bgr_frame;
            cv::cvtColor(yuv_frame, bgr_frame, cv::COLOR_YUV2BGR_I420);

            // 파이 노드는 이미 해상도가 낮으니 리사이징 없이 바로 압축
            cv::imencode(".jpg", bgr_frame, jpg_buffer,
                         {cv::IMWRITE_JPEG_QUALITY, 70});

            // 2. JSON 생성
            json j_pi;
            j_pi["count"] = camData->objects.size();
            for (const auto& obj : camData->objects) {
              j_pi["objs"].push_back(
                  {{"x", obj.x}, {"y", obj.y}, {"w", obj.w}, {"h", obj.h}});
            }
            json_payload = j_pi.dump();
          }
        }

        if (!jpg_buffer.empty()) {
          send_camera_packet(ssl, id_idx++, json_payload, jpg_buffer);
        }
      }
      last_pi_send = std::chrono::steady_clock::now();
    }

    this_thread::sleep_for(chrono::milliseconds(66));  // 15 FPS 유지
  }
}