#include "system_monitor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/statvfs.h>
#include <unistd.h>

using namespace std;

/* ─────────────────────────────────────────
   CPU 사용률
   /proc/stat을 0.5초 간격으로 두 번 읽어서
   변화량으로 계산
───────────────────────────────────────── */

/**
 * @brief /proc/stat에서 idle 시간과 전체 시간을 읽습니다.
 * @param idle  idle 시간 저장 참조
 * @param total 전체 시간 저장 참조
 */
static void read_cpu_stat(long long& idle, long long& total)
{
    ifstream fp("/proc/stat");
    if (!fp.is_open()) {
        idle = total = 0;
        return;
    }

    string cpu;
    long long user, nice, system, idle_val, iowait, irq, softirq;
    fp >> cpu >> user >> nice >> system >> idle_val >> iowait >> irq >> softirq;

    idle  = idle_val + iowait;
    total = user + nice + system + idle_val + iowait + irq + softirq;
}

/**
 * @brief /proc/stat을 0.5초 간격으로 두 번 읽어 CPU 사용률을 계산합니다.
 *        사용률 = (전체 변화량 - idle 변화량) / 전체 변화량 * 100
 * @return CPU 사용률 (0.0 ~ 100.0)
 */
float get_cpu_usage()
{
    long long idle1, total1, idle2, total2;

    read_cpu_stat(idle1, total1);
    usleep(500000); /* 0.5초 대기 */
    read_cpu_stat(idle2, total2);

    long long total_diff = total2 - total1;
    long long idle_diff  = idle2  - idle1;

    if (total_diff == 0) return 0.0f;

    return static_cast<float>(total_diff - idle_diff) / static_cast<float>(total_diff) * 100.0f;
}

/* ─────────────────────────────────────────
   CPU 온도
   /sys/class/thermal/thermal_zone0/temp
   값이 밀리도(℃ * 1000) 단위로 저장됨
───────────────────────────────────────── */

/**
 * @brief 라즈베리파이 CPU 온도를 읽습니다.
 * @return CPU 온도 (°C), 실패 시 -1.0
 */
float get_cpu_temp()
{
    ifstream fp("/sys/class/thermal/thermal_zone0/temp");
    if (!fp.is_open()) return -1.0f;

    int temp_raw = 0;
    fp >> temp_raw;

    return static_cast<float>(temp_raw) / 1000.0f; /* 밀리도 → 도 변환 */
}

/* ─────────────────────────────────────────
   디스크 사용률
   statvfs()로 루트 파티션(/) 정보 읽기
   사용률 = (전체 - 여유) / 전체 * 100
───────────────────────────────────────── */

/**
 * @brief 루트 파티션(/)의 디스크 사용률을 계산합니다.
 * @return 디스크 사용률 (0.0 ~ 100.0), 실패 시 -1.0
 */
float get_disk_usage()
{
    struct statvfs stat;
    if (statvfs("/", &stat) != 0) return -1.0f;

    unsigned long long total = stat.f_blocks * stat.f_frsize;
    unsigned long long free  = stat.f_bfree  * stat.f_frsize;
    unsigned long long used  = total - free;

    if (total == 0) return 0.0f;

    return static_cast<float>(used) / static_cast<float>(total) * 100.0f;
}

/* ─────────────────────────────────────────
   한 번에 읽기
───────────────────────────────────────── */

/**
 * @brief CPU 사용률, CPU 온도, 디스크 사용률을 한 번에 읽어 반환합니다.
 * @return SystemStats 구조체
 */
SystemStats get_system_stats()
{
    SystemStats stats;
    stats.cpu_usage  = get_cpu_usage();
    stats.cpu_temp   = get_cpu_temp();
    stats.disk_usage = get_disk_usage();
    return stats;
}
