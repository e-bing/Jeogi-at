#include "communication.hpp"
#include "config_loader.h"
#include "../../protocol/message_types.hpp"
#include <fcntl.h>
#include <iostream>
#include <cmath>
#include <unistd.h>
#include <termios.h>
#include <chrono>
#include <nlohmann/json.hpp>
#include <mqtt/async_client.h>

using json = nlohmann::json;
using namespace std;

mutex g_uart_mutex;

static const string COMM_CLIENT_ID = "motor_pi_comm_pub";

static mqtt::async_client* g_comm_mqtt   = nullptr;
static string               g_sensor_topic;
static bool                 g_mqtt_connected = false;

// 프로토콜 상수 - uart_protocol.h (STM32) 와 동일한 값 사용
static constexpr uint8_t PKT_STX          = 0xAA;
static constexpr uint8_t PKT_ETX          = 0x55;

// CMD 정의
static constexpr uint8_t CMD_GET_CO       = 0x01; // CO 센서 값 요청
static constexpr uint8_t CMD_GET_CO2      = 0x02; // CO2 센서 값 요청
static constexpr uint8_t CMD_GET_TEMP_HUM = 0x03; // 온습도 전송 (Pi → STM32)
static constexpr uint8_t CMD_TRAIN_DEST   = 0x09; // 열차 목적지 전송
static constexpr uint8_t CMD_SET_LED      = 0x10; // LED(혼잡도) 일괄 전송
static constexpr uint8_t CMD_DISPLAY_CTRL = 0x11; // 디스플레이 제어 명령
static constexpr uint8_t CMD_PLAY_WAV     = 0x20; // WAV 파일 재생 명령
static constexpr uint8_t CMD_GET_WAVS     = 0x21; // WAV 파일 목록 요청
static constexpr uint8_t CMD_RESP_WAVS    = 0x22; // WAV 파일 목록 응답
static constexpr uint8_t CMD_RESP_SENSOR  = 0x80; // 센서 응답 (STM32 → Pi)
static constexpr uint8_t CMD_ACK          = 0xF0; // 명령 수신 확인
static constexpr uint8_t CMD_NACK         = 0xF1; // 명령 처리 실패
// NACK 에러 코드
static constexpr uint8_t ERR_INVALID_CMD  = 0x01;
static constexpr uint8_t ERR_INVALID_DATA = 0x02;
static constexpr uint8_t ERR_DEVICE_BUSY  = 0x03;

/* ═══════════════════════════════════════════
   내부 유틸리티
═══════════════════════════════════════════ */

/**
 * @brief CRC 계산 (STM32와 동일: cmd ^ len ^ data XOR ^ ETX)
 */
static uint8_t calc_crc(uint8_t cmd, uint8_t len, const uint8_t* data) {
    uint8_t crc = cmd ^ len;
    for (int i = 0; i < len; i++) crc ^= data[i];
    crc ^= PKT_ETX;
    return crc;
}

/**
 * @brief 타임아웃 기반 1바이트 수신
 */
static bool read_byte_timeout(int fd, uint8_t& b,
                               const chrono::steady_clock::time_point& t0,
                               int timeout_ms) {
    while (true) {
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(
            chrono::steady_clock::now() - t0).count();
        if (elapsed > timeout_ms) return false;
        ssize_t n = read(fd, &b, 1);
        if (n > 0) return true;
        if (n < 0 && errno != EINTR) return false;
    }
}

/* ═══════════════════════════════════════════
   초기화
═══════════════════════════════════════════ */

/**
 * @brief MQTT 초기화. config.json에서 브로커 주소와 토픽을 읽어옵니다.
 */
void init_comm_mqtt() {
    try {
        auto config    = load_config();
        string broker  = config.value("mqtt_broker",  "tcp://localhost:1883");
        g_sensor_topic = config.value("sensor_topic", Protocol::MQTT_TOPIC_AIR_QUALITY);

        g_comm_mqtt = new mqtt::async_client(broker, COMM_CLIENT_ID);

        mqtt::connect_options opts;
        opts.set_keep_alive_interval(20);
        opts.set_clean_session(true);
        opts.set_automatic_reconnect(true);

        g_comm_mqtt->connect(opts)->wait();
        g_mqtt_connected = true;
        cout << "✅ MQTT 연결 완료: " << broker << endl;
    } catch (const mqtt::exception& e) {
        cerr << "❌ MQTT 연결 실패: " << e.what() << endl;
    }
}

/**
 * @brief UART 초기화. 115200 baud, 8N1, non-blocking.
 */
int init_comm_uart(const string& device) {
    int fd = open(device.c_str(), O_RDWR | O_NOCTTY);
    if (fd < 0) {
        cerr << "❌ UART 열기 실패: " << device << endl;
        return -1;
    }

    struct termios options;
    tcgetattr(fd, &options);
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | INLCR);
    options.c_oflag &= ~OPOST;
    options.c_cc[VMIN]  = 0;
    options.c_cc[VTIME] = 0;
    tcsetattr(fd, TCSANOW, &options);
    tcflush(fd, TCIOFLUSH);
    fcntl(fd, F_SETFL, O_NONBLOCK);

    cout << "✅ UART 초기화 완료: " << device << " (115200 baud)" << endl;
    return fd;
}

/* ═══════════════════════════════════════════
   UART 공용 함수
═══════════════════════════════════════════ */

/**
 * @brief STX~ETX 길이 기반 패킷 수신 + CRC 검증
 * @param out 수신된 전체 프레임 [STX][CMD][LEN][DATA...][CRC][ETX]
 * @return CRC 검증 통과 시 true
 */
bool read_packet(int fd, vector<uint8_t>& out, int timeout_ms) {
    out.clear();
    auto t0 = chrono::steady_clock::now();
    uint8_t b = 0;

    // STX 찾기
    while (true) {
        if (!read_byte_timeout(fd, b, t0, timeout_ms)) return false;
        if (b == PKT_STX) break;
    }

    uint8_t cmd = 0, len = 0;
    if (!read_byte_timeout(fd, cmd, t0, timeout_ms)) return false;
    if (!read_byte_timeout(fd, len, t0, timeout_ms)) return false;

    vector<uint8_t> data(len);
    for (int i = 0; i < len; i++) {
        if (!read_byte_timeout(fd, data[i], t0, timeout_ms)) return false;
    }

    uint8_t crc_rx = 0, etx = 0;
    if (!read_byte_timeout(fd, crc_rx, t0, timeout_ms)) return false;
    if (!read_byte_timeout(fd, etx,    t0, timeout_ms)) return false;

    if (etx != PKT_ETX) {
        cerr << "⚠️ ETX mismatch: " << hex << (int)etx << dec << endl;
        return false;
    }

    uint8_t crc_exp = calc_crc(cmd, len, len ? data.data() : nullptr);
    if (crc_rx != crc_exp) {
        cerr << "⚠️ CRC mismatch: recv=" << hex << (int)crc_rx
             << " exp=" << (int)crc_exp << dec << endl;
        return false;
    }

    out.push_back(PKT_STX);
    out.push_back(cmd);
    out.push_back(len);
    out.insert(out.end(), data.begin(), data.end());
    out.push_back(crc_rx);
    out.push_back(PKT_ETX);
    return true;
}

/* ═══════════════════════════════════════════
   MQTT 공용 함수
═══════════════════════════════════════════ */

/**
 * @brief 임의 토픽으로 MQTT 메시지를 발행합니다.
 */
void publish_to_server(const string& topic, const string& payload) {
    if (!g_mqtt_connected || !g_comm_mqtt) return;
    try {
        g_comm_mqtt->publish(topic, payload, 1, false)->wait();
    } catch (const mqtt::exception& e) {
        cerr << "❌ publish_to_server 실패 [" << topic << "]: " << e.what() << endl;
    }
}

/* ═══════════════════════════════════════════
   펌웨어 → 서버 (MQTT publish)
═══════════════════════════════════════════ */

/**
 * @brief CO/CO2 센서값을 MQTT로 서버에 전송합니다.
 */
void send_to_server_sensor(float co, float co2) {
    json payload = {
        {Protocol::FIELD_CO, co},
        {Protocol::FIELD_CO2, co2}
    };
    publish_to_server(g_sensor_topic, payload.dump());
    cout << "📤 [→서버] CO: " << co << " ppm | CO2: " << co2 << " ppm" << endl;
}

/* ═══════════════════════════════════════════
   펌웨어 → STM32 (UART send)
═══════════════════════════════════════════ */

/**
 * @brief STM32에 CO 센서값을 요청합니다. (CMD 0x01)
 */
bool send_to_stm32_get_co(int uart_fd, float& out_co) {
    if (uart_fd < 0) return false;
    lock_guard<mutex> lock(g_uart_mutex);
    tcflush(uart_fd, TCIFLUSH);
    uint8_t req[] = {0xAA, 0x01, 0x00, 0x54, 0x55};
    write(uart_fd, req, sizeof(req));
    vector<uint8_t> rx;
    if (read_packet(uart_fd, rx, 300) && rx.size() >= 9 && rx[2] >= 4 && rx[4] == CMD_GET_CO) {
        out_co = (float)((rx[5] << 8) | rx[6]) / 100.0f;
        return true;
    }
    return false;
}

/**
 * @brief STM32에 CO2 센서값을 요청합니다. (CMD 0x02)
 */
bool send_to_stm32_get_co2(int uart_fd, float& out_co2) {
    if (uart_fd < 0) return false;
    lock_guard<mutex> lock(g_uart_mutex);
    tcflush(uart_fd, TCIFLUSH);
    uint8_t req[] = {0xAA, 0x02, 0x00, 0x57, 0x55};
    write(uart_fd, req, sizeof(req));
    vector<uint8_t> rx;
    if (read_packet(uart_fd, rx, 300) && rx.size() >= 9 && rx[2] >= 4 && rx[4] == CMD_GET_CO2) {
        out_co2 = (float)((rx[5] << 8) | rx[6]) / 100.0f;
        return true;
    }
    return false;
}

/**
 * @brief 온습도 데이터를 STM32에 전송합니다. (CMD 0x03)
 */
void send_to_stm32_temp_humi(int uart_fd, float temp, float humi) {
    if (uart_fd < 0) return;
    lock_guard<mutex> lock(g_uart_mutex);

    int16_t temp_raw = static_cast<int16_t>(round(temp * 100));
    int16_t humi_raw = static_cast<int16_t>(round(humi * 100));

    uint8_t data[4] = {
        static_cast<uint8_t>((temp_raw >> 8) & 0xFF),
        static_cast<uint8_t>(temp_raw        & 0xFF),
        static_cast<uint8_t>((humi_raw >> 8) & 0xFF),
        static_cast<uint8_t>(humi_raw        & 0xFF)
    };

    uint8_t pkt[9] = {
        PKT_STX, CMD_GET_TEMP_HUM, 0x04,
        data[0], data[1], data[2], data[3],
        calc_crc(CMD_GET_TEMP_HUM, 0x04, data),
        PKT_ETX
    };

    write(uart_fd, pkt, sizeof(pkt));
    cout << "📤 [→STM32] Temp: " << temp << "°C | Humi: " << humi << "%" << endl;
}

/**
 * @brief WAV 파일 재생 명령을 STM32에 전송합니다. (CMD 0x20)
 */
void send_to_stm32_play_wav(int uart_fd, const string& filename) {
    if (uart_fd < 0 || filename.size() > 255) return;
    lock_guard<mutex> lock(g_uart_mutex);

    uint8_t len = static_cast<uint8_t>(filename.size());
    vector<uint8_t> pkt;
    pkt.push_back(PKT_STX);
    pkt.push_back(CMD_PLAY_WAV);
    pkt.push_back(len);
    for (char c : filename) pkt.push_back(static_cast<uint8_t>(c));
    pkt.push_back(calc_crc(CMD_PLAY_WAV, len, len ? (uint8_t*)filename.data() : nullptr));
    pkt.push_back(PKT_ETX);

    write(uart_fd, pkt.data(), pkt.size());
    tcdrain(uart_fd);
    cout << "📤 [→STM32] PLAY_WAV: " << filename << endl;
}

/**
 * @brief WAV 파일 목록을 STM32에 요청합니다. (CMD 0x21)
 */
vector<string> send_to_stm32_get_wavs(int uart_fd) {
    if (uart_fd < 0) return {};
    lock_guard<mutex> lock(g_uart_mutex);

    uint8_t pkt[5] = {
        PKT_STX, CMD_GET_WAVS, 0x00,
        calc_crc(CMD_GET_WAVS, 0, nullptr),
        PKT_ETX
    };

    tcflush(uart_fd, TCIFLUSH);
    write(uart_fd, pkt, sizeof(pkt));
    tcdrain(uart_fd);
    cout << "📤 [→STM32] GET_WAVS" << endl;

    vector<uint8_t> rx;
    if (!read_packet(uart_fd, rx, 10000)) {
        cerr << "❌ GET_WAVS 응답 타임아웃" << endl;
        return {};
    }

    if (rx[1] != CMD_RESP_WAVS) {
        cerr << "❌ GET_WAVS 잘못된 응답 CMD: " << hex << (int)rx[1] << dec << endl;
        return {};
    }

    // 수신 패킷 raw hex 덤프
    printf("[RECV] %zu bytes: ", rx.size());
    for (auto v : rx) printf("%02X ", v);
    printf("\n");

    uint8_t data_len = rx[2];
    string s(reinterpret_cast<const char*>(&rx[3]), data_len);

    vector<string> files;
    string cur;
    for (char c : s) {
        if (c == '\n') {
            if (!cur.empty()) { files.push_back(cur); cur.clear(); }
        } else if (c != '\r' && c != '\0') {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) files.push_back(cur);

    printf("[RECV] files(%zu)\n", files.size());
    return files;
}

/**
 * @brief 전체 구역 혼잡도를 STM32에 한 번에 전송합니다. (CMD 0x10)
 *        display.cpp의 DisplayCallback에서 MQTT 수신 시 호출됩니다.
 */
void send_to_stm32_bulk_congestion(int uart_fd, const vector<int>& levels) {
    if (uart_fd < 0 || levels.empty() || levels.size() > 8) return;
    lock_guard<mutex> lock(g_uart_mutex);

    vector<uint8_t> payload;
    payload.reserve(levels.size());
    for (int level : levels) {
        payload.push_back(static_cast<uint8_t>(level));
    }

    uint8_t len = static_cast<uint8_t>(payload.size());
    uint8_t crc = calc_crc(CMD_SET_LED, len, payload.data());

    vector<uint8_t> pkt;
    pkt.reserve(payload.size() + 5);
    pkt.push_back(PKT_STX);
    pkt.push_back(CMD_SET_LED);
    pkt.push_back(len);
    pkt.insert(pkt.end(), payload.begin(), payload.end());
    pkt.push_back(crc);
    pkt.push_back(PKT_ETX);

    write(uart_fd, pkt.data(), pkt.size());
    tcdrain(uart_fd);
    cout << "📤 [→STM32] BULK_CONGESTION: " << (int)len << "구역" << endl;
}

void send_to_stm32_display_control(int uart_fd, const string& action) {
    if (uart_fd < 0 || action.empty() || action.size() > 255) return;
    lock_guard<mutex> lock(g_uart_mutex);

    uint8_t len = static_cast<uint8_t>(action.size());
    vector<uint8_t> pkt;
    pkt.push_back(PKT_STX);
    pkt.push_back(CMD_DISPLAY_CTRL);
    pkt.push_back(len);
    for (char c : action) pkt.push_back(static_cast<uint8_t>(c));
    pkt.push_back(calc_crc(CMD_DISPLAY_CTRL, len, len ? (uint8_t*)action.data() : nullptr));
    pkt.push_back(PKT_ETX);

    write(uart_fd, pkt.data(), pkt.size());
    tcdrain(uart_fd);
    cout << "📤 [→STM32] DISPLAY_CTRL: " << action << endl;
}

void send_to_stm32_display_screen(int uart_fd, int screen) {
    if (screen < 0 || screen > 9) return;
    send_to_stm32_display_control(uart_fd, to_string(screen));
}

void send_to_stm32_train_dest(int uart_fd, uint8_t dest_code) {
    if (uart_fd < 0) return;
    lock_guard<mutex> lock(g_uart_mutex);

    uint8_t payload[1] = {dest_code};
    uint8_t pkt[6] = {
        PKT_STX,
        CMD_TRAIN_DEST,
        0x01,
        payload[0],
        calc_crc(CMD_TRAIN_DEST, 0x01, payload),
        PKT_ETX
    };

    write(uart_fd, pkt, sizeof(pkt));
    tcdrain(uart_fd);
    cout << "📤 [→STM32] TRAIN_DEST: " << static_cast<int>(dest_code) << endl;
}
