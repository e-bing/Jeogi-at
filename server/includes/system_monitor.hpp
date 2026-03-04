#pragma once

#include <string>

/**
 * @brief 단일 디바이스 시스템 상태 구조체
 */
struct DeviceStats {
    std::string device;      // 디바이스 이름 ("firmware" | "server")
    float cpu_usage;         // CPU 사용률 (%)
    float cpu_temp;          // CPU 온도 (°C)
    float disk_usage;        // 디스크 사용률 (%)
    bool  valid = false;     // 데이터 수신 여부
};

/**
 * @brief server 자체 시스템 정보를 읽습니다.
 * @return DeviceStats 구조체
 */
DeviceStats get_server_stats();

/**
 * @brief MQTT 연결 및 system/firmware 구독을 시작합니다.
 */
void init_system_monitor();

/**
 * @brief server + firmware 합산 데이터를 JSON으로 변환해 SSL로 Qt에 전송합니다.
 * @param ssl Qt 클라이언트 SSL 포인터
 */
void send_system_monitor(void* ssl);
