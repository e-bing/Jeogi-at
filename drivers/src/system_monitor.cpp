#include "system_monitor.hpp"
#include <iostream>
#include <fstream>
#include <sys/statvfs.h>
#include <unistd.h>

using namespace std;

/* ─────────────────────────────────────────
   서버(현재 기기) 시스템 정보 측정
───────────────────────────────────────── */

static void read_cpu_stat(long long& idle, long long& total) {
    ifstream fp("/proc/stat");
    if (!fp.is_open()) { idle = total = 0; return; }
    string cpu;
    long long user, nice, system, idle_val, iowait, irq, softirq;
    fp >> cpu >> user >> nice >> system >> idle_val >> iowait >> irq >> softirq;
    idle  = idle_val + iowait;
    total = user + nice + system + idle_val + iowait + irq + softirq;
}

/**
 * @brief 현재 기기(펌웨어)의 시스템 정보를 측정합니다.
 * CPU 사용률은 0.5초 간격으로 측정합니다.
 */
DeviceStats get_server_stats() {
    DeviceStats stats;
    stats.device = "firmware";

    // CPU 사용률 (0.5초 간격)
    long long i1, t1, i2, t2;
    read_cpu_stat(i1, t1);
    usleep(500000);
    read_cpu_stat(i2, t2);
    float diff_t = static_cast<float>(t2 - t1);
    stats.cpu_usage = (diff_t > 0) ? (1.0f - (i2 - i1) / diff_t) * 100.0f : 0.0f;

    // CPU 온도
    ifstream t_fp("/sys/class/thermal/thermal_zone0/temp");
    int raw_t = 0;
    if (t_fp.is_open()) t_fp >> raw_t;
    stats.cpu_temp = raw_t / 1000.0f;

    // 디스크 사용률
    struct statvfs vfs;
    if (statvfs("/", &vfs) == 0 && vfs.f_blocks > 0) {
        stats.disk_usage = (1.0f - (float)vfs.f_bfree / vfs.f_blocks) * 100.0f;
    }

    stats.valid = true;
    return stats;
}