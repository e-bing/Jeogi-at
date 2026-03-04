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