#include "../includes/congestion_publisher.hpp"

using json = nlohmann::json;

CongestionPublisher::CongestionPublisher(CongestionAnalyzer& analyzer)
    : m_analyzer(analyzer), m_running(false) {}

CongestionPublisher::~CongestionPublisher() { stop(); }

void CongestionPublisher::start() {
  if (m_running) return;
  m_running = true;
  m_thread = std::thread(&CongestionPublisher::run, this);
}

void CongestionPublisher::stop() {
  m_running = false;
  if (m_thread.joinable()) {
    m_thread.join();
  }
}

void CongestionPublisher::run() {
  mqtt::async_client client(SERVER_ADDRESS, CLIENT_ID);
  mqtt::connect_options connOpts;
  connOpts.set_keep_alive_interval(20);
  connOpts.set_clean_session(true);

  try {
    std::cout << "[MQTT] Connecting to broker..." << std::endl;
    client.connect(connOpts)->wait();
    std::cout << "[MQTT] Connected. Topic: " << TOPIC << std::endl;

    while (m_running) {
      // 1. Analyzer로부터 데이터 가져오기 (내부 Mutex로 안전함)
      std::vector<int> levels = m_analyzer.getCongestionLevels();

      // 2. JSON 데이터 구성
      json j;
      j["levels"] = levels;
      j["zone_count"] = levels.size();
      j["update_time"] = std::chrono::system_clock::to_time_t(
          std::chrono::system_clock::now());

      // 3. 메시지 발행 (비동기)
      std::string payload = j.dump();
      client.publish(TOPIC, payload, 1, false);

      // 4. 1초 주기 유지
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    client.disconnect()->wait();
  } catch (const mqtt::exception& exc) {
    std::cerr << "❌ MQTT Error: " << exc.what() << std::endl;
  }
}