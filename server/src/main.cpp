#include <chrono>   // FPS 측정용
#include <iomanip>  // FPS 측정용
#include <thread>
#include <vector>

// #include "hanwha_node.hpp"  // 기존 한화 노드 헤더
#include "../includes/pi_node.hpp"
#include "../includes/shared_data.hpp"

int main() {
  // 1. 노드 객체 생성
  PiNode piNode("192.168.0.34");  // 라즈베리 파이 IP
  // HanwhaNode hwNode("192.168.0.50");  // 한화 CCTV IP

  // 3. SDL 메인 렌더링 루프 (UI 스레드)
  // SDL 초기화 및 윈도우 생성
  SDL_Init(SDL_INIT_VIDEO);
  SDL_Window* window =
      SDL_CreateWindow("Unified AI Monitor", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1280, 480, SDL_WINDOW_SHOWN);
  SDL_Renderer* renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  SDL_Texture* piTexture = SDL_CreateTexture(
      renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, 640, 480);

  // 2. 각 노드를 개별 스레드에서 실행
  // 영상 수신 및 데이터 파싱을 병렬로 처리합니다.
  std::thread piThread([&piNode]() { piNode.run(); });
  // std::thread hwThread(&HanwhaNode::run, &hwNode);

  bool running = true;
  SDL_Event ev;

  // FPS 측정용 변수
  int frame_count = 0;
  auto last_time = std::chrono::steady_clock::now();
  float current_fps = 0.0f;
  // 나중에 삭제하기

  while (running) {
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) running = false;
    }
    SDL_RenderClear(renderer);

    // FPS 측정용
    bool frame_updated = false;

    {
      std::lock_guard<std::mutex> lock(g_frame_mutex);
      if (!g_pi_frame_buffer.empty()) {
        int w = 640;
        int h = 480;
        const uint8_t* y_plane = g_pi_frame_buffer.data();
        const uint8_t* u_plane = y_plane + (w * h);
        const uint8_t* v_plane = u_plane + (w * h / 4);

        SDL_UpdateYUVTexture(piTexture, nullptr, y_plane, w, u_plane, w / 2,
                             v_plane, w / 2);

        frame_updated = true;       // FPS 측정용
        g_pi_frame_buffer.clear();  // FPS 측정용
      }
    }

    // === FPS 측정 로직 시작 ===
    if (frame_updated) {
      frame_count++;
      auto now = std::chrono::steady_clock::now();
      std::chrono::duration<double> elapsed = now - last_time;

      if (elapsed.count() >= 1.0) {  // 1초마다 출력
        current_fps = frame_count / elapsed.count();
        std::cout << "[Live Monitor] PI Node FPS: " << std::fixed
                  << std::setprecision(1) << current_fps << std::endl;

        frame_count = 0;
        last_time = now;
      }
    }
    // === FPS 측정 로직 끝 ===

    SDL_Rect piRect = {640, 0, 640, 480};
    SDL_RenderCopy(renderer, piTexture, nullptr, &piRect);

    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);  // 초록색 박스
    {
      std::lock_guard<std::mutex> lock(g_data_mutex);  // MQTT 데이터 뮤텍스
      for (const auto& obj : g_shared_objects) {
        SDL_Rect r = {
            (int)(obj.x + 640),  // 영상이 오른쪽 절반에 있으므로 640 더하기
            (int)obj.y, (int)obj.w, (int)obj.h};
        SDL_RenderDrawRect(renderer, &r);
      }
    }

    SDL_RenderPresent(renderer);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  running = false;  // 루프 종료 신호
  if (piThread.joinable()) {
    piThread.join();  // detach 대신 join으로 스레드가 끝날 때까지 기다려줌
  }
  // hwThread.detach();

  SDL_Quit();
  return 0;
}