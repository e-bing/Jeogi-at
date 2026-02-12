// sensor.cpp
#include "Sensor.h"
#include "Database.h"
#include <iostream>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>

using json = nlohmann::json;
using namespace std;

int init_uart(const char* device) {
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
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;

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

void receive_sensor_data(int uart_fd, MYSQL* conn) {
    char buffer[256];
    string line_buffer = "";

    cout << "🔌 센서 데이터 수신 스레드 시작 (MQ-135 + MQ-7)" << endl << 
	"==================================================" << endl;

    while (true) {
        int bytes = read(uart_fd, buffer, sizeof(buffer) - 1);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            line_buffer += string(buffer);

            size_t pos;
            while ((pos = line_buffer.find('\n')) != string::npos) {
                string line = line_buffer.substr(0, pos);
                line_buffer.erase(0, pos + 1);

                if (line.empty() || line == "\r") continue;

                // JSON 파싱
                try {
                    json data = json::parse(line);

                    // 새로운 JSON 형식: {"sensors":[{"type":"MQ135","co2":123.45},{"type":"MQ7","co":67.89}]}
                    if (data.contains("sensors") && data["sensors"].is_array()) {
                        float co2_value = 0.0;
                        float co_value = 0.0;

                        // 센서 배열 순회
                        for (const auto& sensor : data["sensors"]) {
                            string type = sensor["type"];

                            if (type == "MQ135" && sensor.contains("co2")) {
                                co2_value = sensor["co2"];
                            }
                            else if (type == "MQ7" && sensor.contains("co")) {
                                co_value = sensor["co"];
                            }
                        }

                        // DB 저장
                        save_sensor_data(conn, co_value, co2_value);
                    }
                    // 구버전 호환 (단일 센서 형식)
                    else if (data.contains("sensor") && data.contains("co2")) {
                        float co2 = data["co2"];
                        cout << "📊 수신 (구버전): " << data["sensor"] << " = " << co2 << " ppm" << endl;
                        save_sensor_data(conn, 0.0, co2);
                    }

                } catch (json::exception& e) {
                    // JSON 아닌 라인 무시 (디버깅 메시지 등)
                    cerr << "JSON 파싱 에러: " << e.what() << endl;
                }
            }
        }
        this_thread::sleep_for(chrono::milliseconds(100));
    }
}
