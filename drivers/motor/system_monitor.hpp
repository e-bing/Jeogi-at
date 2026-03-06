#pragma once

#include <string>

/**
 * @brief 시스템 모니터링 데이터 구조체
 */
struct SystemStats {
    float cpu_usage;    // CPU 사용률 (%)
    float cpu_temp;     // CPU 온도 (°C)
    float disk_usage;   // 디스크 사용률 (%)
};

/**
 * @brief /proc/stat을 읽어 CPU 사용률을 계산합니다.
 * @return CPU 사용률 (0.0 ~ 100.0)
 */
float get_cpu_usage();

/**
 * @brief /sys/class/thermal에서 CPU 온도를 읽습니다.
 * @return CPU 온도 (°C)
 */
float get_cpu_temp();

/**
 * @brief 루트 파티션(/)의 디스크 사용률을 계산합니다.
 * @return 디스크 사용률 (0.0 ~ 100.0)
 */
float get_disk_usage();

/**
 * @brief 세 가지 시스템 정보를 한 번에 읽어 구조체로 반환합니다.
 * @return SystemStats 구조체
 */
SystemStats get_system_stats();
