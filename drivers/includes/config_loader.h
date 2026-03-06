#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include <string>
#include <fstream>
#include <nlohmann/json.hpp>

/**
 * @brief config.json 파일을 읽어 JSON 객체를 반환한다.
 */
inline nlohmann::json load_config() {
    std::ifstream file("config/config.json");
    nlohmann::json config;
    
    if (file.is_open()) {
        try {
            file >> config;
        } catch (const nlohmann::json::parse_error& e) {
            // 파싱 에러 시 빈 객체 유지
        }
    }
    return config;
}

#endif
