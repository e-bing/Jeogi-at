#ifndef CONGESTION_ANALYZER_HPP
#define CONGESTION_ANALYZER_HPP

#include <atomic>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "../../protocol/sensor_thresholds.hpp"
#include "config_manager.hpp"
#include "shared_data.hpp"

struct ZoneConfig {
  int zone_id;
  std::string camera_id;
  float x1, y1, x2, y2;
};

class CongestionAnalyzer {
 public:
  CongestionAnalyzer();
  ~CongestionAnalyzer();

  void start();
  void stop();
  void loadConfig();  // config.json에서 ROI 정보 로드

  // 분석된 8개 구역의 혼잡도(0,1,2)를 반환
  std::vector<int> getCongestionLevels();

 private:
  void run();

    /**
     * @brief 인원 수 → 혼잡도 레벨 변환
     *        Protocol::CONGESTION_EASY_MAX, CONGESTION_NORMAL_MAX 기준 사용
     */
    int calculateLevel(int count);

  bool isInside(const DetectedObject& obj, const ZoneConfig& zone);

    std::vector<ZoneConfig> m_zones;
    std::vector<int>        m_current_levels;  // 혼잡도 레벨
    std::mutex              m_level_mutex;

    std::thread          m_thread;
    std::atomic<bool>    m_running;
};

#endif  // CONGESTION_ANALYZER_HPP