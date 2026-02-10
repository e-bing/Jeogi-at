#include "../includes/shared_data.hpp"

// 전역 변수 정의
std::vector<DetectedObject> g_shared_objects;
std::mutex g_data_mutex;
bool g_program_running = true;
std::mutex g_frame_mutex;
std::vector<uint8_t> g_pi_frame_buffer;