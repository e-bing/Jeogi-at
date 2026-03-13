#ifndef CONFIG_MANAGER_HPP
#define CONFIG_MANAGER_HPP

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class ConfigManager {
 public:
  static json load() {
    std::ifstream file("../config/config.json");
    if (!file.is_open()) return json::object();
    json j;
    try {
      file >> j;
    } catch (...) {
      return json::object();
    }
    return j;
  }

  static void save(const json& j) {
    std::ofstream file("../config/config.json");
    file << j.dump(4);
  }

  static void init_default_rois() {
    json config = load();

    // zones 배열이 없거나 비어있는 경우에만 초기화
    if (!config.contains("zones") || config["zones"].empty()) {
      std::cout << "[INIT] config.json에 ROI 설정이 없어 기본값을 생성합니다."
                << std::endl;

      config["zones"] = json::array({// hanwha (1~4구역, 4분할)
                                     {{"zone_id", 1},
                                      {"camera_id", "hanwha"},
                                      {"roi", {0.0, 0.0, 0.25, 1.0}}},
                                     {{"zone_id", 2},
                                      {"camera_id", "hanwha"},
                                      {"roi", {0.25, 0.0, 0.50, 1.0}}},
                                     {{"zone_id", 3},
                                      {"camera_id", "hanwha"},
                                      {"roi", {0.50, 0.0, 0.75, 1.0}}},
                                     {{"zone_id", 4},
                                      {"camera_id", "hanwha"},
                                      {"roi", {0.75, 0.0, 1.0, 1.0}}},

                                     // CAM_01 (5~6구역, 2분할)
                                     {{"zone_id", 5},
                                      {"camera_id", "CAM_01"},
                                      {"roi", {0.0, 0.0, 0.5, 1.0}}},
                                     {{"zone_id", 6},
                                      {"camera_id", "CAM_01"},
                                      {"roi", {0.5, 0.0, 1.0, 1.0}}},

                                     // CAM_02 (7~8구역, 2분할)
                                     {{"zone_id", 7},
                                      {"camera_id", "CAM_02"},
                                      {"roi", {0.0, 0.0, 0.5, 1.0}}},
                                     {{"zone_id", 8},
                                      {"camera_id", "CAM_02"},
                                      {"roi", {0.5, 0.0, 1.0, 1.0}}}});

      save(config);
      std::cout << "[INIT] 기본 ROI 설정 저장 완료" << std::endl;
    }
  }
};

#endif  // CONFIG_MANAGER_HPP