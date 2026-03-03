#ifndef SHARED_DATA_HPP
#define SHARED_DATA_HPP

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

inline std::string g_mqtt_broker = "tcp://192.168.0.53:1883";

// 감지된 객체 정보 구조체
struct DetectedObject {
  float x, y, w, h;
  std::string typeName;
  int objectId;
};

// --- 라즈베리 파이용 ---
struct CameraData {
  std::vector<uint8_t> frame_buffer;
  std::vector<DetectedObject> objects;
  std::mutex data_mutex;
};

extern std::map<std::string, std::shared_ptr<CameraData>> g_pi_node_map;
extern std::mutex g_node_map_mutex;

// --- 한화 카메라용 ---
extern std::vector<uint8_t> g_hw_frame_buffer;
extern std::vector<DetectedObject> g_hw_objects;
extern std::mutex g_hw_frame_mutex;
extern std::mutex g_hw_data_mutex;

extern bool g_program_running;

#endif  // SHARED_DATA_HPP
