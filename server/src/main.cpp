#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>  // FPS 측정용
#include <csignal>
#include <cstring>
#include <iomanip>  // FPS 측정용
#include <iostream>
#include <thread>
#include <vector>

#include "../includes/config_manager.hpp"
#include "../includes/hanwha_node.hpp"
#include "../includes/pi_node.hpp"
#include "../includes/shared_data.hpp"

// 센서 및 DB 관련 헤더
#include "../includes/database.h"
#include "../includes/qt.h"
#include "../includes/sensor.h"

using namespace std;

volatile sig_atomic_t stop_flag = 0;

void signal_handler(int signum) { stop_flag = 1; }

int main() {
  const int PORT = 12345;  // Qt 통신용 포트

  signal(SIGINT, signal_handler);

  cout << "==================================================" << endl;
  cout << "    AI Camera Monitor & Station Server Start" << endl;
  cout << "==================================================" << endl;

  // -- 서버 초기화 영역 --
  // 1. TLS 및 포트 초기화
  init_tls();
  kill_process_using_port(PORT);

  extern int g_uart_fd;
  g_uart_fd = init_uart("/dev/ttyS0");

  if (g_uart_fd < 0) {
    cerr << "UART 초기화 실패 - 센서 데이터 수신 불가, 모터 제어 불가" << endl;
  } else {
    cout << "✅ STM32 UART 연결 성공: /dev/ttyS0" << endl;
  }

  auto config = ConfigManager::load();
  if (config.empty()) {
    std::cerr << "Config file missing! Run ./setup first.\n";
    return -1;
  }

  // 2. 센서 통신 스레드 시작 (UART + DB)
  thread sensor_thread([]() {
    extern int g_uart_fd;

    if (g_uart_fd < 0) {
      cerr << "⚠️ UART 미연결 - 센서 스레드 종료" << endl;
      return;
    }

    DBConfig config;
    MYSQL* sensor_conn = connect_db(config);
    if (!sensor_conn) {
      cerr << "❌ 센서용 DB 연결 실패" << endl;
      return;
    }

    receive_sensor_data(g_uart_fd, sensor_conn);
    close_db(sensor_conn);
  });
  sensor_thread.detach();

  // 3. Qt 클라이언트 대응 TLS 서버 소켓 생성
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd != -1) {
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) >= 0 &&
        listen(server_fd, 5) >= 0) {
      cout << "📡 TLS 서버 대기 중 (포트 " << PORT << ")" << endl;

      // 서버 수락 루프를 별도 스레드에서 실행 (SDL 루프 방해 방지)
      thread server_accept_thread([server_fd]() {
        while (true) {
          int client_socket = accept(server_fd, NULL, NULL);
          if (client_socket >= 0) {
            cout << "새 클라이언트 연결 시도 → TLS 핸드셰이크 시작" << endl;
            thread(handle_client, client_socket).detach();
          }
        }
      });
      server_accept_thread.detach();
    }
  }

  // 1. 노드 객체 생성
  std::string hw_ip = config["hanwha"]["ip"];
  std::string hw_id = config["hanwha"]["user"];
  std::string hw_pw = config["hanwha"]["pw"];
  std::string hw_profile = config["hanwha"]["profile"];
  std::string hw_url = "rtsp://" + hw_id + ":" + hw_pw + "@" + hw_ip + "/" +
                       hw_profile + "/media.smp";

  HanwhaNode hwNode(hw_url);
  std::vector<std::shared_ptr<PiNode>> pi_nodes;
  std::vector<std::thread> pi_threads;

  if (config.contains("pi_nodes") && config["pi_nodes"].is_array()) {
    for (const auto& item : config["pi_nodes"]) {
      std::string ip = item["ip"];
      std::string id = item["id"];
      std::string topic = item["mqtt_topic"];

      {
        std::lock_guard<std::mutex> lock(g_node_map_mutex);
        g_pi_node_map[id] = std::make_shared<CameraData>();
      }

      auto node = std::make_shared<PiNode>(ip, topic, id);
      pi_nodes.push_back(node);

      pi_threads.emplace_back([node]() { node->run(); });
    }
  }

  // 3. SDL 메인 렌더링 루프 (UI 스레드)
  // SDL 초기화 및 윈도우 생성
  SDL_Init(SDL_INIT_VIDEO);
  SDL_Window* window =
      SDL_CreateWindow("Unified AI Monitor", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1280, 480, SDL_WINDOW_SHOWN);
  SDL_Renderer* renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  SDL_Texture* hwTexture = SDL_CreateTexture(
      renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, 1920, 1080);
  SDL_Texture* piTexture = SDL_CreateTexture(
      renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, 640, 480);

  std::thread hwThread([&hwNode]() { hwNode.run(); });

  bool running = true;
  SDL_Event ev;

  // FPS 측정용 변수
  int frame_count = 0;
  auto last_time = std::chrono::steady_clock::now();
  float current_fps = 0.0f;
  // 나중에 삭제하기

  while (running && !stop_flag) {
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) running = false;
    }
    SDL_RenderClear(renderer);

    // FPS 측정용
    bool frame_updated = false;
    // --- [A] 한화 카메라 렌더링 (왼쪽) ---
    {
      std::lock_guard<std::mutex> lock(g_hw_frame_mutex);
      if (!g_hw_frame_buffer.empty()) {
        int real_w = 1920;
        int real_h = 1080;
        if (g_hw_frame_buffer.size() >= (real_w * real_h * 3 / 2)) {
          SDL_UpdateYUVTexture(
              hwTexture, nullptr, g_hw_frame_buffer.data(), real_w,  // Y pitch
              g_hw_frame_buffer.data() + (real_w * real_h),
              real_w / 2,  // U pitch
              g_hw_frame_buffer.data() + (real_w * real_h * 5 / 4),
              real_w / 2  // V pitch
          );
        }
        g_hw_frame_buffer.clear();
      }
    }
    SDL_Rect hwRect = {0, 0, 640, 480};
    SDL_RenderCopy(renderer, hwTexture, nullptr, &hwRect);

    // 한화 객체 박스
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);  // 빨간색
    {
      std::lock_guard<std::mutex> lock(g_hw_data_mutex);
      for (const auto& obj : g_hw_objects) {
        SDL_Rect r = {(int)(obj.x * 640), (int)(obj.y * 480),
                      (int)(obj.w * 640), (int)(obj.h * 480)};
        SDL_RenderDrawRect(renderer, &r);
      }
    }

    // --- [B] 라즈베리 파이 렌더링 (오른쪽) ---
    {
      std::lock_guard<std::mutex> lock(g_node_map_mutex);
      if (g_pi_node_map.count("CAM_01")) {
        auto& camData = g_pi_node_map["CAM_01"];
        std::lock_guard<std::mutex> dataLock(camData->data_mutex);

        if (!camData->frame_buffer.empty()) {
          int w = 640;
          int h = 480;
          const uint8_t* y_plane = camData->frame_buffer.data();
          const uint8_t* u_plane = y_plane + (w * h);
          const uint8_t* v_plane = u_plane + (w * h / 4);

          SDL_UpdateYUVTexture(piTexture, nullptr, y_plane, w, u_plane, w / 2,
                               v_plane, w / 2);

          frame_updated = true;           // FPS 측정용
          camData->frame_buffer.clear();  // FPS 측정용
        }
      }
    }

    // === FPS 측정 로직 시작 ===
    // if (frame_updated) {
    //   frame_count++;
    //   auto now = std::chrono::steady_clock::now();
    //   std::chrono::duration<double> elapsed = now - last_time;

    //   if (elapsed.count() >= 1.0) {  // 1초마다 출력
    //     current_fps = frame_count / elapsed.count();
    //     std::cout << "[Live Monitor] PI Node FPS: " << std::fixed
    //               << std::setprecision(1) << current_fps << std::endl;

    //     frame_count = 0;
    //     last_time = now;
    //   }
    // }
    // === FPS 측정 로직 끝 ===

    // === 화면에 박스 그리기 ===
    // SDL_Rect piRect = {640, 0, 640, 480};
    // SDL_RenderCopy(renderer, piTexture, nullptr, &piRect);

    // SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);  // 초록색 박스
    // {
    //   std::lock_guard<std::mutex> lock(g_pi_data_mutex);  // MQTT 데이터
    //   뮤텍스 for (const auto& obj : g_pi_shared_objects) {
    //     SDL_Rect r = {
    //         (int)(obj.x + 640),  // 영상이 오른쪽 절반에 있으므로 640 더하기
    //         (int)obj.y, (int)obj.w, (int)obj.h};
    //     SDL_RenderDrawRect(renderer, &r);
    //   }
    // }

    // SDL_RenderPresent(renderer);

    // === 인구 수 터미널에 출력 ===
    int pi_count = 0;
    int hw_count = 0;
    std::string pi_debug_info = "";  // 디버그용 문자열
    {
      std::lock_guard<std::mutex> lock(g_node_map_mutex);
      for (auto const& [id, camData] : g_pi_node_map) {
        std::lock_guard<std::mutex> dataLock(camData->data_mutex);
        pi_count += camData->objects.size();

        pi_debug_info +=
            id + ": " + std::to_string(camData->objects.size()) + " ";
        // (선택 사항) SDL 화면에 모든 노드의 박스를 다 그리고 싶다면 여기서
        // DrawRect 실행
        if (id == "CAM_01") {  // 일단 CAM_01 박스만 화면에 표시
          SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
          for (const auto& obj : camData->objects) {
            SDL_Rect r = {(int)(obj.x + 640), (int)obj.y, (int)obj.w,
                          (int)obj.h};
            SDL_RenderDrawRect(renderer, &r);
          }
        }
      }
    }

    {
      std::lock_guard<std::mutex> lock(g_hw_data_mutex);
      hw_count = g_hw_objects.size();
    }

    // std::cout << "\r[실시간 카운트] Hanwha: " << hw_count
    //           << "명 | Pi: " << pi_count << "명 | 합계: " << hw_count +
    //           pi_count
    //           << "명" << std::flush;
    std::cout << "\r[실시간] Hanwha: " << hw_count << " | "
              << pi_debug_info  // 여기서 CAM_02가 계속 0인지 확인 가능
              << "| 합계: " << hw_count + pi_count << "명" << std::flush;

    // === 인구 수 터미널에 출력 끝 ===

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  running = false;  // 루프 종료 신호
  for (auto& t : pi_threads) {
    if (t.joinable()) t.join();
  }
  if (hwThread.joinable()) hwThread.join();

  close(server_fd);
  cleanup_tls();
  SDL_Quit();

  return 0;
}
