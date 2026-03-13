#include "../includes/congestion_analyzer.hpp"

CongestionAnalyzer::CongestionAnalyzer() : m_running(false) {
  m_current_levels.assign(8, 0);
}

CongestionAnalyzer::~CongestionAnalyzer() { stop(); }

void CongestionAnalyzer::loadConfig() {
  json config = ConfigManager::load();
  std::lock_guard<std::mutex> lock(m_level_mutex);
  m_zones.clear();

  if (config.contains("zones") && config["zones"].is_array()) {
    for (const auto &item : config["zones"]) {
      ZoneConfig z;
      z.zone_id = item.value("zone_id", 0);
      z.camera_id = item.value("camera_id", "");
      z.x1 = item["roi"][0];
      z.y1 = item["roi"][1];
      z.x2 = item["roi"][2];
      z.y2 = item["roi"][3];
      m_zones.push_back(z);
    }
    std::cout << "[Analyzer] ROI Config Loaded. (" << m_zones.size()
              << " zones)\n";
  }
}

bool CongestionAnalyzer::isInside(const DetectedObject &obj,
                                  const ZoneConfig &zone) {
  // 중심점(Center Point) 기준 판별
  float centerX = obj.x + (obj.w / 2.0f);
  float centerY = obj.y + (obj.h / 2.0f);

  return (centerX >= zone.x1 && centerX <= zone.x2 && centerY >= zone.y1 &&
          centerY <= zone.y2);
}

int CongestionAnalyzer::calculateLevel(int count) {
  if (count < Protocol::CONGESTION_EASY_MAX)
    return 0;
  if (count < Protocol::CONGESTION_NORMAL_MAX)
    return 1;
  return 2;
}

void CongestionAnalyzer::start() {
  if (m_running)
    return;
  loadConfig();
  m_running = true;
  m_thread = std::thread(&CongestionAnalyzer::run, this);
}

void CongestionAnalyzer::stop() {
  m_running = false;
  if (m_thread.joinable()) {
    m_thread.join();
  }
}

void CongestionAnalyzer::run() {
  while (m_running) {
    std::vector<int> temp_counts(8, 0);

    // 1. 한화 카메라 객체들 체크
    {
      std::lock_guard<std::mutex> lock(g_hw_data_mutex);
      for (const auto &obj : g_hw_objects) {
        for (const auto &zone : m_zones) {
          if (zone.camera_id == "hanwha" && isInside(obj, zone)) {
            if (zone.zone_id >= 1 && zone.zone_id <= 8)
              temp_counts[zone.zone_id - 1]++;
          }
        }
      }
    }

    // 2. 라즈베리 파이 노드들 체크
    {
      std::lock_guard<std::mutex> lock(g_node_map_mutex);
      for (const auto &[id, cameraData] : g_pi_node_map) {
        std::lock_guard<std::mutex> dataLock(cameraData->data_mutex);
        for (const auto &obj : cameraData->objects) {
          for (const auto &zone : m_zones) {
            if (zone.camera_id == id && isInside(obj, zone)) {
              if (zone.zone_id >= 1 && zone.zone_id <= 8)
                temp_counts[zone.zone_id - 1]++;
            }
          }
        }
      }
    }

    // 3. 혼잡도 레벨 변환 및 저장
    {
      std::lock_guard<std::mutex> lock(m_level_mutex);
      for (int i = 0; i < 8; ++i) {
        m_current_levels[i] = calculateLevel(temp_counts[i]);
      }
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

std::vector<int> CongestionAnalyzer::getCongestionLevels() {
  std::lock_guard<std::mutex> lock(m_level_mutex);
  return m_current_levels;
}