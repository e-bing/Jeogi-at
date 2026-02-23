// sensor.cpp
#include "sensor.h"
#include "database.h"
#include "motor.h"
#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <termios.h>
#include <thread>
#include <unistd.h>

using json = nlohmann::json;
using namespace std;

int g_uart_fd = -1;

extern bool g_auto_mode;

int init_uart(const char *device) {
  int uart_fd = open(device, O_RDWR | O_NOCTTY);
  if (uart_fd < 0) {
    cerr << "UART 열기 실패: " << device << endl;
    return -1;
  }

  struct termios options;
  tcgetattr(uart_fd, &options);

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

  // ✅ 블로킹 모드로 변경 - 최소 1바이트 받을 때까지 대기
  options.c_cc[VMIN] = 1;     // 최소 1바이트
  options.c_cc[VTIME] = 10;   // 1초 타임아웃 (10 * 0.1초)

  tcsetattr(uart_fd, TCSANOW, &options);
  tcflush(uart_fd, TCIOFLUSH);

  cout << "✅ UART 초기화 완료: " << device << " (115200 baud)" << endl;
  return uart_fd;
}

void close_uart(int uart_fd) {
    if (uart_fd >= 0) {
        close(uart_fd);
        cout << "UART 연결 종료" << endl;
    }
}

void receive_sensor_data(int uart_fd, MYSQL *conn) {
    char buffer[256];
    string line_buffer = "";

    cout << "🔌 센서 데이터 수신 시작 (통합 출력 모드)" << endl;
    cout << "--------------------------------------------------" << endl;

    if (uart_fd < 0) return;

    const auto WINDOW_MS = 2000;
    bool has_co = false, has_co2 = false;
    float last_co = 0.0f, last_co2 = 0.0f;
    auto last_co_time = chrono::steady_clock::time_point();
    auto last_co2_time = chrono::steady_clock::time_point();

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
                    auto now = chrono::steady_clock::now();

                    if (data["type"] == "MQ135") {
                        last_co2 = data["value"].get<float>();
                        last_co2_time = now;
                        has_co2 = true;
                    } else if (data["type"] == "MQ7") {
                        last_co = data["value"].get<float>();
                        last_co_time = now;
                        has_co = true;
                    }

                    // 1. 두 데이터가 모두 도착했을 때 통합 출력
                    if (has_co && has_co2) {
                        auto diff = chrono::duration_cast<chrono::milliseconds>(
                            last_co_time > last_co2_time ? last_co_time - last_co2_time : last_co2_time - last_co_time).count();
			  
                        if (diff <= WINDOW_MS) {
                            cout << "📊 [통합 데이터] CO: " << last_co << " ppm | CO2: " << last_co2 << " ppm" << endl;
                            save_sensor_data(conn, last_co, last_co2);
			    publish_sensor_mqtt(last_co, last_co2);
                            has_co = has_co2 = false;
                        }
                    }
                } catch (json::exception &e) {
                    cerr << "⚠️ JSON 에러: " << e.what() << endl;
                }
            }
        }

        // 2. 타임아웃 처리 (한쪽 데이터만 들어오고 2초 지났을 때)
        auto now = chrono::steady_clock::now();
        if (has_co && !has_co2 && chrono::duration_cast<chrono::milliseconds>(now - last_co_time).count() > WINDOW_MS) {
            cout << "📊 [단독 데이터] CO: " << last_co << " ppm | CO2: (데이터 없음)" << endl;
            save_sensor_data(conn, last_co, 0.0f);
            has_co = false;
        }
        if (has_co2 && !has_co && chrono::duration_cast<chrono::milliseconds>(now - last_co2_time).count() > WINDOW_MS) {
            cout << "📊 [단독 데이터] CO: (데이터 없음) | CO2: " << last_co2 << " ppm" << endl;
            save_sensor_data(conn, 0.0f, last_co2);
            has_co2 = false;
        }
    }
}
