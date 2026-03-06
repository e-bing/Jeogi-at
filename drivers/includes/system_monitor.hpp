// system_monitor.hpp
#pragma once

#include <string>

/**
 * @brief 단일 디바이스 시스템 상태 구조체
 */
struct DeviceStats {
    std::string device;      // 디바이스 이름 ("firmware" | "server")
    float cpu_usage;         // CPU 사용률 (%)
    float cpu_temp;          // CPU 온도 (℃)
    float disk_usage;        // 디스크 사용률 (%)
    bool  valid = false;     // 데이터 유효성 여부
};

/**
 * @brief 서버(현재 기기)의 시스템 정보를 측정합니다.
 */
DeviceStats get_server_stats();

/**
 * @brief 시스템 모니터링을 위한 MQTT 구독 및 설정을 시작합니다.
 */
void init_system_monitor();

/**
 * @brief 서버와 펌웨어의 상태를 합산하여 Qt 클라이언트로 전송합니다.
 * @param ssl Qt 연결 SSL 핸들
 */
void send_system_monitor(void* ssl);