#ifndef SHARED_DATA_HPP
#define SHARED_DATA_HPP

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

// 감지된 객체 정보 구조체
struct DetectedObject {
  float x, y, w, h;
  std::string typeName;
  int objectId;
};

// --- 라즈베리 파이용 ---
extern std::vector<DetectedObject> g_pi_shared_objects;
extern std::vector<uint8_t> g_pi_frame_buffer;
extern std::mutex g_pi_data_mutex;
extern std::mutex g_pi_frame_mutex;

// --- 한화 카메라용 ---
extern std::vector<uint8_t> g_hw_frame_buffer;
extern std::vector<DetectedObject> g_hw_objects;
extern std::mutex g_hw_frame_mutex;
extern std::mutex g_hw_data_mutex;

extern bool g_program_running;

#endif  // SHARED_DATA_HPP
