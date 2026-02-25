#include "sensor.h"
#include "motor.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <nlohmann/json.hpp>
#include <mqtt/async_client.h>
#include <chrono>

using json = nlohmann::json;
using namespace std;

int g_uart_fd = -1;

// MQTT publisher for forwarding sensor data to server
static const string SENSOR_BROKER    = "tcp://192.168.0.53:1883";
static const string SENSOR_CLIENT_ID = "motor_pi_sensor_pub";
static mqtt::async_client g_sensor_mqtt(SENSOR_BROKER, SENSOR_CLIENT_ID);
static bool g_sensor_mqtt_connected = false;

// 프로토콜 상수
static constexpr uint8_t PKT_STX         = 0xAA;
static constexpr uint8_t PKT_ETX         = 0x55;
static constexpr uint8_t CMD_GET_CO      = 0x01;
static constexpr uint8_t CMD_GET_CO2     = 0x02;
static constexpr uint8_t CMD_RESP_SENSOR = 0x80;
static constexpr uint8_t CMD_NACK        = 0xF1;
static constexpr int     PKT_MAX_DATA    = 16;

// 패킷 구조체
struct Packet {
    uint8_t cmd;
    uint8_t len;
    uint8_t data[PKT_MAX_DATA];
    uint8_t crc;
};

// 상태머신
typedef enum {
    RX_WAIT_STX, RX_RECV_CMD, RX_RECV_LEN,
    RX_RECV_DATA, RX_RECV_CRC, RX_WAIT_ETX
} RxState;

// CRC
static uint8_t calc_crc(uint8_t cmd, uint8_t len, const uint8_t* data) {
    uint8_t crc = cmd ^ len ^ PKT_ETX;
    for (int i = 0; i < len; i++) crc ^= data[i];
    return crc;
}

// 파싱
static bool parse_byte(uint8_t byte, RxState& state, Packet& pkt, uint8_t& idx) {
    switch (state) {
        case RX_WAIT_STX: if (byte == PKT_STX) state = RX_RECV_CMD; break;
        case RX_RECV_CMD: pkt.cmd = byte; state = RX_RECV_LEN; break;
        case RX_RECV_LEN:
            pkt.len = byte; idx = 0;
            state = (byte > 0) ? RX_RECV_DATA : RX_RECV_CRC; break;
        case RX_RECV_DATA:
            if (idx < PKT_MAX_DATA) pkt.data[idx++] = byte;
            if (idx >= pkt.len) state = RX_RECV_CRC; break;
        case RX_RECV_CRC: pkt.crc = byte; state = RX_WAIT_ETX; break;
        case RX_WAIT_ETX:
            state = RX_WAIT_STX;
            if (byte == PKT_ETX && calc_crc(pkt.cmd, pkt.len, pkt.data) == pkt.crc)
                return true;
            break;
    }
    return false;
}

/**
 * @brief STM32와의 UART 연결을 초기화합니다.
 * @param device UART 장치 경로 (예: "/dev/ttyS0")
 * @return 성공 시 파일 디스크립터, 실패 시 -1
 */
int init_uart(const char* device) {
    int fd = open(device, O_RDWR | O_NOCTTY);
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

/**
 * @brief UART 연결을 종료합니다.
 * @param uart_fd 닫을 파일 디스크립터
 */
void close_uart(int uart_fd) {
    if (uart_fd >= 0) {
        close(uart_fd);
        cout << "UART 연결 종료" << endl;
    }
}

/**
 * @brief 서버로 센서 데이터를 전송하기 위한 MQTT 연결을 초기화합니다.
 */
static void init_sensor_mqtt() {
    try {
        mqtt::connect_options opts;
        opts.set_keep_alive_interval(20);
        opts.set_clean_session(true);
        opts.set_automatic_reconnect(true);
        g_sensor_mqtt.connect(opts)->wait();
        g_sensor_mqtt_connected = true;
        cout << "✅ 센서 MQTT 연결 완료 (서버로 전송용)" << endl;
    } catch (const mqtt::exception& e) {
        cerr << "❌ 센서 MQTT 연결 실패: " << e.what() << endl;
    }
}

/**
 * @brief MQTT를 통해 서버로 센서 데이터를 전송합니다.
 * @param co  CO 농도 (ppm)
 * @param co2 CO2 농도 (ppm)
 */
static void publish_to_server(float co, float co2) {
    if (!g_sensor_mqtt_connected) return;
    try {
        json payload = {{"co", co}, {"co2", co2}};
        g_sensor_mqtt.publish("sensor/data", payload.dump(), 1, false)->wait();
        cout << "📤 서버로 센서값 전송 CO: " << co << " | CO2: " << co2 << endl;
    } catch (const mqtt::exception& e) {
        cerr << "❌ 센서 MQTT 전송 실패: " << e.what() << endl;
    }
}

/**
 * @brief UART를 통해 STM32로부터 센서 데이터를 수신합니다.
 *        자동 모드 시 센서값으로 모터를 제어하고,
 *        항상 서버로 센서값을 MQTT로 전송합니다.
 * @param uart_fd UART 장치 파일 디스크립터
 */

void receive_sensor_data(int uart_fd) {
    if (uart_fd < 0) return;

    // 서버 전송을 위한 MQTT 클라이언트 연결 초기화
    init_sensor_mqtt();

    cout << "프로토콜 시작 - 1초마다 GET_CO, GET_CO2 전송 및 서버 발행" << endl;

    uint8_t byte;
    RxState state;
    Packet  pkt;
    uint8_t dataIdx;

    while (true) {
        float current_co = 0.0f;
        float current_co2 = 0.0f;

        // 1. GET_CO 패킷 전송 및 응답 수신
        // [AA][01][00][54][55] 전송
        uint8_t req_co[] = {0xAA, CMD_GET_CO, 0x00, 0x54, 0x55};
        write(uart_fd, req_co, sizeof(req_co));

        state = RX_WAIT_STX; dataIdx = 0;
        auto t_start = chrono::steady_clock::now();

        // 300ms 동안 응답 대기
        while (chrono::steady_clock::now() - t_start < chrono::milliseconds(300)) {
            if (read(uart_fd, &byte, 1) > 0) {
                if (parse_byte(byte, state, pkt, dataIdx)) {
                    // 응답 패킷 식별 및 데이터 추출
                    if (pkt.data[1] == CMD_GET_CO) {
                        current_co = (float)((pkt.data[2] << 8) | pkt.data[3]) / 100.0f;
                        break;
                    }
                }
            }
        }

        // 2. GET_CO2 패킷 전송 및 응답 수신
        // [AA][02][00][57][55] 전송 (CRC = 0x02 ^ 0x00 ^ 0x55 = 0x57)
        uint8_t req_co2[] = {0xAA, CMD_GET_CO2, 0x00, 0x57, 0x55};
        write(uart_fd, req_co2, sizeof(req_co2));

        state = RX_WAIT_STX; dataIdx = 0;
        t_start = chrono::steady_clock::now();

        // 300ms 동안 응답 대기
        while (chrono::steady_clock::now() - t_start < chrono::milliseconds(300)) {
            if (read(uart_fd, &byte, 1) > 0) {
                if (parse_byte(byte, state, pkt, dataIdx)) {
                    // 응답 패킷 식별 및 데이터 추출
                    if (pkt.data[1] == CMD_GET_CO2) {
                        current_co2 = (float)((pkt.data[2] << 8) | pkt.data[3]) / 100.0f;
                        break;
                    }
                }
            }
        }

        // 3. 서버로 JSON 데이터 전송
        publish_to_server(current_co, current_co2);
	
	// 4. 모터 제어 동작 수정
        // g_auto_mode가 true(자동 모드)일 때만 센서값으로 모터를 제어한다.
        extern bool g_auto_mode; // motor.cpp에 선언된 전역 변수를 참조
        if (g_auto_mode) {
            auto_motor_control(current_co2, current_co);
        }
	
        // 약 1초 주기를 맞추기 위해 남은 시간(400ms) 대기
        this_thread::sleep_for(chrono::milliseconds(400));
    }
}
