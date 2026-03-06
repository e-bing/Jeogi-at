#ifndef CONGESTION_PUBLISHER_HPP
#define CONGESTION_PUBLISHER_HPP

#include <mqtt/async_client.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>

#include "congestion_analyzer.hpp"

class CongestionPublisher {
 public:
  CongestionPublisher(CongestionAnalyzer& analyzer);
  ~CongestionPublisher();

  void start();
  void stop();

 private:
  void run();

  CongestionAnalyzer& m_analyzer;
  std::thread m_thread;
  std::atomic<bool> m_running;

  // MQTT 설정
  const std::string SERVER_ADDRESS{"tcp://localhost:1883"};
  const std::string CLIENT_ID{"MainServer_Congestion_Cpp"};
  const std::string TOPIC{Protocol::MQTT_TOPIC_CONGESTION_STATUS};
};

#endif  // CONGESTION_PUBLISHER_HPP