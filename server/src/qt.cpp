// qt.cpp
#include "../includes/qt.h"

#include "../includes/system_monitor.h"

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

void enqueue_camera_packet(std::queue<SendPacket>& q, std::mutex& mtx,
                           std::condition_variable& cv, uint32_t cam_id,
                           const string& json_str,
                           const vector<unsigned char>& img_data) {
  CamProtocol::PacketHeader header;
  header.magic = htonl(CamProtocol::MAGIC_COOKIE);
  header.camera_id = htonl(cam_id);
  header.json_size = htonl(static_cast<uint32_t>(json_str.size()));
  header.image_size = htonl(static_cast<uint32_t>(img_data.size()));

  SendPacket pkt;
  pkt.type = SendPacket::Type::CAMERA;
  pkt.data.resize(sizeof(header) + json_str.size() + img_data.size());
  memcpy(pkt.data.data(), &header, sizeof(header));
  memcpy(pkt.data.data() + sizeof(header), json_str.data(), json_str.size());
  memcpy(pkt.data.data() + sizeof(header) + json_str.size(), img_data.data(),
         img_data.size());

  {
    lock_guard<mutex> lock(mtx);
    q.push(std::move(pkt));
  }
  cv.notify_one();  // writer_thread 깨우기
}

void writer_thread_func(SSL* ssl, bool* connected, std::queue<SendPacket>& q,
                        std::mutex& mtx, std::condition_variable& cv) {
  while (*connected) {
    std::unique_lock<mutex> lock(mtx);
    cv.wait_for(lock, chrono::milliseconds(50),
                [&] { return !q.empty() || !*connected; });

    while (!q.empty()) {
      SendPacket pkt = std::move(q.front());
      q.pop();
      lock.unlock();  // write 중엔 락 해제

      // SSL_write는 이 스레드에서만!
      int total = 0;
      int size = (int)pkt.data.size();
      while (total < size) {
        int n = SSL_write(ssl, pkt.data.data() + total, size - total);
        if (n <= 0) {
          *connected = false;
          return;
        }
        total += n;
      }

      lock.lock();
    }
  }
}

void video_streaming_worker(bool* client_connected, std::queue<SendPacket>& q,
                            std::mutex& mtx, std::condition_variable& cv) {
  auto last_pi_send = chrono::steady_clock::now();
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
          cv::resize(bgr_frame, resized_frame, cv::Size(320, 240));

          // [Step C] JPEG 압축 (압축률 80% 정도가 적당)
          cv::imencode(".jpg", resized_frame, jpg_buffer,
                       {cv::IMWRITE_JPEG_QUALITY, 15});

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
        enqueue_camera_packet(q, mtx, cv, 1, json_payload, jpg_buffer);
      }
    }

    // 2. Pi Node 카메라들 (ID: 2~, 5 FPS)
    // 서브 카메라는 시간을 체크하여 200ms마다 전송
    auto now = chrono::steady_clock::now();
    if (now - last_pi_send >= chrono::milliseconds(200)) {
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
          auto now = chrono::steady_clock::now();
          if (now - last_pi_send >= chrono::milliseconds(200)) {
            enqueue_camera_packet(q, mtx, cv, id_idx, json_payload, jpg_buffer);
            last_pi_send = now;
          }
        }
        id_idx++;
      }
      last_pi_send = now;
    }

    this_thread::sleep_for(chrono::milliseconds(20));  // 테스트
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
  int snd_size = 1024 * 1024;  // 1MB로 확장
  setsockopt(client_socket, SOL_SOCKET, SO_SNDBUF, &snd_size, sizeof(snd_size));

  std::queue<SendPacket> send_queue;
  std::mutex queue_mutex;
  std::condition_variable queue_cv;
  bool client_connected = true;

  // --- 1. 전송 전담 스레드 (소비자) 시작 ---
  thread w_thread(writer_thread_func, ssl, &client_connected,
                  std::ref(send_queue), std::ref(queue_mutex),
                  std::ref(queue_cv));

  // --- 2. 영상 처리 스레드 (생산자) 시작 ---
  // 주의: 이제 video_streaming_worker는 전송 함수가 아닌 enqueue 함수를 써야 함
  thread v_thread(video_streaming_worker, &client_connected,
                  std::ref(send_queue), std::ref(queue_mutex),
                  std::ref(queue_cv));

  // 논블로킹 모드 설정 (명령 수신 대기 중 서버 멈춤 방지)
  int flags = fcntl(client_socket, F_GETFL, 0);
  fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);

  string cmd_buffer = "";

  bool client_connected = true;
  int db_tick = 0;
  int sys_tick = 0;

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
    } else {
      int err = SSL_get_error(ssl, bytes);
      if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        this_thread::sleep_for(chrono::milliseconds(5));
        continue;
      } else {
        cout << "📡 클라이언트 소켓 연결 종료 감지 (Code: " << err << ")"
             << endl;
        client_connected = false;
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
      vector<string> messages = {s_zones};  // 전송할 메시지 목록 정의
      bool write_failed = false;            // 변수 선언 추가

      lock_guard<mutex> lock(g_ssl_send_mutex);

      for (const string& s : messages) {
        int total_sent = 0;
        while (total_sent < (int)s.length()) {
          int n =
              SSL_write(ssl, s.c_str() + total_sent, s.length() - total_sent);
          if (n <= 0) {
            int err = SSL_get_error(ssl, n);
            if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ) {
              this_thread::sleep_for(chrono::milliseconds(1));
              continue;
            }
            write_failed = true;
            break;
          }
          total_sent += n;
        }
        if (write_failed) break;
      }
      if (write_failed) {
        client_connected = false;
        break;
      }
    } catch (...) {
    }

    // [실시간 혼잡도 전송 - enqueue 방식]
    if (db_tick % 10 == 0) {  // 너무 자주 큐에 쌓이지 않게 조절
      json msg_zones = {{"type", "zone_congestion"},
                        {"zones", g_analyzer.getCongestionLevels()},
                        {"total_count", get_total_people_count()}};

      SendPacket pkt;
      pkt.type = SendPacket::Type::JSON;
      string s = msg_zones.dump() + "\n";
      pkt.data.assign(s.begin(), s.end());

      {
        lock_guard<mutex> lock(queue_mutex);
        send_queue.push(std::move(pkt));
      }
      queue_cv.notify_one();
    }

    // [DB 기반 데이터 전송] - db_tick이 500이 될 때마다 (약 1~2초 간격)
    if (++db_tick >= 500) {
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

    if (++sys_tick >= 500) {  // 5초마다
      sys_tick = 0;
      lock_guard<mutex> lock(g_ssl_send_mutex);
      send_system_monitor(ssl);
    }
    this_thread::sleep_for(chrono::milliseconds(10));
  }

  client_connected = false;

  queue_cv.notify_all();  // 대기 중인 스레드 깨우기
  if (w_thread.joinable()) w_thread.join();
  if (v_thread.joinable()) v_thread.join();

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
