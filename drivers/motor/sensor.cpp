#include "sensor.h"
#include "motor.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <nlohmann/json.hpp>
#include <mqtt/async_client.h>

using json = nlohmann::json;
using namespace std;

int g_uart_fd = -1;

// MQTT publisher for forwarding sensor data to server
static const string SENSOR_BROKER    = "tcp://192.168.0.53:1883";
static const string SENSOR_CLIENT_ID = "motor_pi_sensor_pub";
static mqtt::async_client g_sensor_mqtt(SENSOR_BROKER, SENSOR_CLIENT_ID);
static bool g_sensor_mqtt_connected = false;

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
    options.c_cc[VMIN]  = 1;
    options.c_cc[VTIME] = 10;
    tcsetattr(fd, TCSANOW, &options);
    tcflush(fd, TCIOFLUSH);

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
    char buffer[256];
    string line_buffer = "";

    init_sensor_mqtt();

    cout << "🔌 STM32 센서 데이터 수신 시작" << endl;
    cout << "--------------------------------------------------" << endl;

    if (uart_fd < 0) return;

    float last_co  = 0.0f;
    float last_co2 = 0.0f;

    while (true) {
        int bytes = read(uart_fd, buffer, sizeof(buffer) - 1);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            line_buffer += string(buffer);

            size_t pos;
            while ((pos = line_buffer.find('\n')) != string::npos) {
                string line = line_buffer.substr(0, pos);
                line_buffer.erase(0, pos + 1);

                try {
                    json data = json::parse(line);
                    string type  = data.value("type", "");
                    float  value = data.value("value", 0.0f);

                    if (type == "MQ135") {
                        last_co2 = value;
                        cout << "📊 CO2: " << last_co2 << " ppm" << endl;
                    } else if (type == "MQ7") {
                        last_co = value;
                        cout << "📊 CO: " << last_co << " ppm" << endl;
                    }

                    // Forward to server
                    publish_to_server(last_co, last_co2);

                    // Auto mode: control motor based on sensor values
                    if (g_auto_mode) {
                        auto_motor_control(last_co2, last_co);
                    }

                } catch (json::exception& e) {
                    cerr << "⚠️ JSON 에러: " << e.what() << endl;
                }
            }
        }
    }
}
