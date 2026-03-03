#include "system_monitor.h"
#include "../includes/shared_data.hpp"
#include <iostream>
#include <fstream>
#include <sys/statvfs.h>
#include <unistd.h>
#include <mutex>
#include <nlohmann/json.hpp>
#include <openssl/ssl.h>
#include <mqtt/async_client.h>

using json = nlohmann::json;
using namespace std;

/* ─────────────────────────────────────────
   MQTT 클라이언트
   브로커 주소는 shared_data.hpp의 g_mqtt_broker 사용
───────────────────────────────────────── */
static const string CLIENT_ID = "server_sys_monitor";

static mqtt::async_client g_sys_mqtt(g_mqtt_broker, CLIENT_ID);
static bool g_sys_mqtt_connected = false;

/* ─────────────────────────────────────────
   firmware로부터 수신한 데이터 캐시
   MQTT 콜백에서 업데이트, send_system_monitor에서 읽음
───────────────────────────────────────── */
static DeviceStats g_firmware_stats;
static mutex       g_firmware_mutex;

/* ─────────────────────────────────────────
   server 자체 시스템 정보 읽기
───────────────────────────────────────── */

/**
 * @brief /proc/stat에서 idle 시간과 전체 시간을 읽습니다.
 */
static void read_cpu_stat(long long& idle, long long& total)
{
    ifstream fp("/proc/stat");
    if (!fp.is_open()) { idle = total = 0; return; }

    string cpu;
    long long user, nice, system, idle_val, iowait, irq, softirq;
    fp >> cpu >> user >> nice >> system >> idle_val >> iowait >> irq >> softirq;

    idle  = idle_val + iowait;
    total = user + nice + system + idle_val + iowait + irq + softirq;
}

/**
 * @brief /proc/stat을 0.5초 간격으로 두 번 읽어 CPU 사용률을 계산합니다.
 *        사용률 = (전체 변화량 - idle 변화량) / 전체 변화량 * 100
 */
static float get_cpu_usage()
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

/**
 * @brief /sys/class/thermal에서 CPU 온도를 읽습니다.
 *        파일 값은 밀리도(℃ * 1000) 단위이므로 1000으로 나눕니다.
 */
static float get_cpu_temp()
{
    ifstream fp("/sys/class/thermal/thermal_zone0/temp");
    if (!fp.is_open()) return -1.0f;

    int temp_raw = 0;
    fp >> temp_raw;
    return static_cast<float>(temp_raw) / 1000.0f;
}

/**
 * @brief 루트 파티션(/)의 디스크 사용률을 계산합니다.
 *        사용률 = (전체 - 여유) / 전체 * 100
 */
static float get_disk_usage()
{
    struct statvfs stat;
    if (statvfs("/", &stat) != 0) return -1.0f;

    unsigned long long total = stat.f_blocks * stat.f_frsize;
    unsigned long long free  = stat.f_bfree  * stat.f_frsize;
    unsigned long long used  = total - free;
    if (total == 0) return 0.0f;

    return static_cast<float>(used) / static_cast<float>(total) * 100.0f;
}

/**
 * @brief server 자체 시스템 정보를 읽어 DeviceStats로 반환합니다.
 */
DeviceStats get_server_stats()
{
    DeviceStats stats;
    stats.device     = "server";
    stats.cpu_usage  = get_cpu_usage();
    stats.cpu_temp   = get_cpu_temp();
    stats.disk_usage = get_disk_usage();
    stats.valid      = true;
    return stats;
}

/* ─────────────────────────────────────────
   MQTT 콜백 - firmware로부터 시스템 상태 수신
───────────────────────────────────────── */

/**
 * @brief system/firmware 토픽 수신 시 JSON을 파싱해 캐시를 업데이트합니다.
 *
 * 수신 JSON 형식:
 * {"device":"firmware","cpu_usage":23.5,"cpu_temp":47.2,"disk_usage":61.3}
 */
class SystemMonitorCallback : public mqtt::callback {
public:
    void message_arrived(mqtt::const_message_ptr msg) override {
        try {
            json data = json::parse(msg->get_payload_str());

            lock_guard<mutex> lock(g_firmware_mutex);
            g_firmware_stats.device     = data.value("device",    "firmware");
            g_firmware_stats.cpu_usage  = data.value("cpu_usage",  0.0f);
            g_firmware_stats.cpu_temp   = data.value("cpu_temp",   0.0f);
            g_firmware_stats.disk_usage = data.value("disk_usage", 0.0f);
            g_firmware_stats.valid      = true;

            cout << "📥 firmware 시스템 상태 수신: "
                 << "CPU "  << g_firmware_stats.cpu_usage  << "% | "
                 << "Temp " << g_firmware_stats.cpu_temp   << "°C | "
                 << "Disk " << g_firmware_stats.disk_usage << "%" << endl;

        } catch (json::exception& e) {
            cerr << "❌ 시스템 상태 파싱 실패: " << e.what() << endl;
        }
    }
};

static SystemMonitorCallback g_sys_cb;

/* ─────────────────────────────────────────
   초기화 - MQTT 연결 및 구독
───────────────────────────────────────── */

/**
 * @brief MQTT 연결 후 system/firmware 토픽을 구독합니다.
 *        브로커 주소는 shared_data.hpp의 g_mqtt_broker를 사용합니다.
 */
void init_system_monitor()
{
    try {
        mqtt::connect_options opts;
        opts.set_keep_alive_interval(20);
        opts.set_clean_session(true);
        opts.set_automatic_reconnect(true);

        g_sys_mqtt.connect(opts)->wait();
        g_sys_mqtt_connected = true;
        g_sys_mqtt.set_callback(g_sys_cb);
        g_sys_mqtt.subscribe("system/firmware", 1)->wait();
        cout << "✅ 시스템 모니터 MQTT 연결 완료" << endl;
    } catch (const mqtt::exception& e) {
        cerr << "❌ 시스템 모니터 MQTT 연결 실패: " << e.what() << endl;
    }
}

/* ─────────────────────────────────────────
   Qt로 전송
   server + firmware 합산 JSON을 SSL_write로 전송
───────────────────────────────────────── */

/**
 * @brief server와 firmware의 시스템 상태를 합산해 Qt로 전송합니다.
 *
 * 전송 JSON 형식:
 * {
 *   "type": "system_monitor",
 *   "server":  { "cpu_usage": 15.2, "cpu_temp": 52.1, "disk_usage": 45.0 },
 *   "firmware": { "cpu_usage": 23.5, "cpu_temp": 47.2, "disk_usage": 61.3, "connected": true }
 * }
 */
void send_system_monitor(void* ssl)
{
    if (!ssl) return;

    // 1. server 자체 정보 읽기
    DeviceStats server = get_server_stats();

    // 2. firmware 캐시 읽기
    DeviceStats firmware;
    {
        lock_guard<mutex> lock(g_firmware_mutex);
        firmware = g_firmware_stats;
    }

    // 3. JSON 조립
    json payload = {
        {"type", "system_monitor"},
        {"server", {
            {"cpu_usage",  server.cpu_usage},
            {"cpu_temp",   server.cpu_temp},
            {"disk_usage", server.disk_usage}
        }},
        {"firmware", {
            {"cpu_usage",  firmware.valid ? firmware.cpu_usage  : -1.0f},
            {"cpu_temp",   firmware.valid ? firmware.cpu_temp   : -1.0f},
            {"disk_usage", firmware.valid ? firmware.disk_usage : -1.0f},
            {"connected",  firmware.valid}
        }}
    };

    // 4. SSL로 Qt에 전송
    string msg = payload.dump() + "\n";
    SSL_write(static_cast<SSL*>(ssl), msg.c_str(), msg.length());

    cout << "📤 Qt로 시스템 상태 전송 완료" << endl;
}
