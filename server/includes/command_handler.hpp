// command_handler.hpp
#ifndef COMMAND_HANDLER_HPP
#define COMMAND_HANDLER_HPP

#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "audio.hpp"
#include "config_manager.hpp"
#include "congestion_analyzer.hpp"
#include "display.hpp"
#include "motor.hpp"

// Qt 클라이언트로부터 수신한 JSON 명령을 파싱하고 처리합니다.
// 지원 타입 - device_command (mode_control / motor / audio / lighting /
// digital_display)
// 지원 타입 - roi_update (camera_id / zone_id / roi)
void handle_qt_command(const std::string& cmd_str);

extern std::atomic<bool> g_roi_updated;

#endif  // COMMAND_HANDLER_HPP