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

extern std::vector<DetectedObject> g_shared_objects;
extern std::vector<uint8_t> g_pi_frame_buffer;
extern std::mutex g_data_mutex;
extern std::mutex g_frame_mutex;
extern bool g_program_running;

#endif  // SHARED_DATA_HPP
