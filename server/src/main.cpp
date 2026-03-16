#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <csignal>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

#include "../includes/config_manager.hpp"
#include "../includes/congestion_analyzer.hpp"
#include "../includes/congestion_publisher.hpp"
#include "../includes/hanwha_node.hpp"
#include "../includes/pi_node.hpp"
#include "../includes/shared_data.hpp"

// 센서, 모터 및 DB 관련 헤더
#include "../includes/audio.hpp"
#include "../includes/client_handler.hpp"
#include "../includes/database.hpp"
#include "../includes/display.hpp"
#include "../includes/motor.hpp"
#include "../includes/sensor.hpp"

// 모니터링 헤더
#include "../includes/system_monitor.hpp"

using namespace std;

volatile sig_atomic_t stop_flag = 0;
CongestionAnalyzer g_analyzer;

void signal_handler(int signum) {
  stop_flag = 1;
  exit(0);
}

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

  auto config = ConfigManager::load();
  if (config.empty()) {
    std::cerr << "Config file missing! Run ./setup first.\n";
    return -1;
  }
  if (config.contains("mqtt")) {
    g_mqtt_broker = config["mqtt"]["broker"];
    cout << "✅ MQTT 브로커 설정: " << g_mqtt_broker << endl;
  }
  ConfigManager::init_default_rois();

  // 브로커 주소 설정 후 MQTT 초기화
  init_mqtt_motor();
  init_mqtt_audio();
  init_mqtt_display();
  init_system_monitor();

  // 2. 센서 통신 스레드 시작 (UART + DB)
  g_analyzer.start();

  thread sensor_thread([]() {
    MYSQL* sensor_conn = connect_db();
    if (!sensor_conn) {
      cerr << "❌ 센서용 DB 연결 실패" << endl;
      return;
    }

    receive_sensor_data(sensor_conn);

    close_db(sensor_conn);
  });

  // 펌웨어 라즈베리파이 측 전송 스레드 생성
  CongestionPublisher congestion_pub(g_analyzer);
  congestion_pub.start();

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

  std::thread hwThread([&hwNode]() { hwNode.run(); });
  cout << "\n서버 동작 중... (종료: Ctrl+C)" << endl;

  bool running = true;
  while (running && !stop_flag) {
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // === 인구 수 터미널에 출력 ===
    auto levels =
        g_analyzer
            .getCongestionLevels();  // 현재 8개 구역의 레벨(0,1,2) 가져오기

    int total_pi = 0;
    {
      lock_guard<mutex> lock(g_node_map_mutex);
      for (auto const& [id, camData] : g_pi_node_map) {
        total_pi += camData->objects.size();
      }
    }
  }

  running = false;  // 루프 종료 신호

  cout << "\nStopping Server..." << endl;
  if (sensor_thread.joinable()) sensor_thread.join();
  for (auto& t : pi_threads) {
    if (t.joinable()) t.join();
  }
  if (hwThread.joinable()) hwThread.join();

  congestion_pub.stop();
  g_analyzer.stop();

  close(server_fd);
  cleanup_tls();

  return 0;
}
