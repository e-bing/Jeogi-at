#include "../includes/shared_data.hpp"

// 전역 변수 정의
std::map<std::string, std::shared_ptr<CameraData>> g_pi_node_map;
std::mutex g_node_map_mutex;

std::vector<uint8_t> g_hw_frame_buffer;
std::vector<DetectedObject> g_hw_objects;
std::mutex g_hw_frame_mutex;
std::mutex g_hw_data_mutex;

bool g_program_running = true;