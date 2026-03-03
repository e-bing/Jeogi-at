#ifndef CONFIG_MANAGER_HPP
#define CONFIG_MANAGER_HPP

#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class ConfigManager {
 public:
  static json load() {
    std::ifstream file("../config/config.json");
    if (!file.is_open()) return json::object();
    json j;
    file >> j;
    return j;
  }

  static void save(const json& j) {
    std::ofstream file("../config/config.json");
    file << j.dump(4);
  }
};

#endif  // CONFIG_MANAGER_HPP