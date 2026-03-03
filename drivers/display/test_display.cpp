#include <chrono>
#include <csignal>
#include <cstdint>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <mqtt/async_client.h>

using json = nlohmann::json;
using namespace std;

static constexpr uint8_t PKT_STX = 0xAA;
static constexpr uint8_t PKT_ETX = 0x55;
static constexpr uint8_t CMD_BULK_CONGESTION = 0x10;

static const string TOPIC_CONGESTION = "iot/server/status/congestion";
static volatile sig_atomic_t g_running = 1;

struct AppConfig {
    string broker_ip = "192.168.0.48";
    int broker_port = 1883;
    string client_id = "display_pi_sub";
    string uart_device = "/dev/ttyS0";
};

static uint8_t calc_crc(uint8_t cmd, uint8_t len, const uint8_t* data) {
    uint8_t crc = cmd ^ len ^ PKT_ETX;
    for (int i = 0; i < len; ++i) crc ^= data[i];
    return crc;
}

static AppConfig load_config(const string& path) {
    AppConfig config;
    ifstream ifs(path);
    if (!ifs.is_open()) {
        cerr << "⚠️ config 파일 열기 실패: " << path << " (기본값 사용)" << endl;
        return config;
    }

    try {
        json j;
        ifs >> j;
        config.broker_ip = j.value("broker_ip", config.broker_ip);
        config.broker_port = j.value("broker_port", config.broker_port);
        config.client_id = j.value("client_id", config.client_id);
        config.uart_device = j.value("uart_device", config.uart_device);
    } catch (const exception& e) {
        cerr << "⚠️ config 파싱 실패: " << e.what() << " (기본값 사용)" << endl;
    }
    return config;
}

static int init_uart(const char* device) {
    int fd = open(device, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("UART Open Failed");
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
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;
    tcsetattr(fd, TCSANOW, &options);
    tcflush(fd, TCIOFLUSH);

    cout << "✅ UART 초기화 완료: " << device << " (115200 baud)" << endl;
    return fd;
}

static bool send_congestion_packet(int uart_fd, const vector<int>& levels) {
    if (levels.empty() || levels.size() > 8) {
        cerr << "❌ UART 전송 실패: levels 크기 오류(" << levels.size() << ")" << endl;
        return false;
    }

    vector<uint8_t> payload;
    payload.reserve(levels.size());
    for (int level : levels) {
        if (level < 0 || level > 2) {
            cerr << "❌ UART 전송 실패: levels 값 오류(" << level << ")" << endl;
            return false;
        }
        payload.push_back(static_cast<uint8_t>(level));
    }

    const uint8_t len = static_cast<uint8_t>(payload.size());
    const uint8_t crc = calc_crc(CMD_BULK_CONGESTION, len, payload.data());

    vector<uint8_t> packet;
    packet.reserve(payload.size() + 5);
    packet.push_back(PKT_STX);
    packet.push_back(CMD_BULK_CONGESTION);
    packet.push_back(len);
    packet.insert(packet.end(), payload.begin(), payload.end());
    packet.push_back(crc);
    packet.push_back(PKT_ETX);

    const ssize_t written = write(uart_fd, packet.data(), packet.size());
    if (written != static_cast<ssize_t>(packet.size())) {
        cerr << "❌ UART 전송 실패: write(" << written << "/" << packet.size() << ")" << endl;
        return false;
    }
    tcdrain(uart_fd);
    return true;
}

class DisplayMqttCallback : public mqtt::callback {
public:
    explicit DisplayMqttCallback(int uart_fd) : uart_fd_(uart_fd) {}

    void message_arrived(mqtt::const_message_ptr msg) override {
        if (msg->get_topic() != TOPIC_CONGESTION) return;

        try {
            json data = json::parse(msg->get_payload_str());
            if (!data.contains("update_time") || !data["update_time"].is_number_integer()) {
                cerr << "❌ JSON 오류: update_time 누락/타입 오류" << endl;
                return;
            }
            if (!data.contains("levels") || !data["levels"].is_array()) {
                cerr << "❌ JSON 오류: levels 누락/타입 오류" << endl;
                return;
            }

            const long long update_time = data["update_time"].get<long long>();
            const auto now = chrono::system_clock::to_time_t(chrono::system_clock::now());
            const long long lag_sec = static_cast<long long>(now) - update_time;

            vector<int> levels;
            levels.reserve(data["levels"].size());
            for (const auto& v : data["levels"]) {
                if (!v.is_number_integer()) {
                    cerr << "❌ JSON 오류: levels 원소 타입 오류" << endl;
                    return;
                }
                levels.push_back(v.get<int>());
            }

            if (data.contains("zone_count")) {
                if (!data["zone_count"].is_number_integer()) {
                    cerr << "❌ JSON 오류: zone_count 타입 오류" << endl;
                    return;
                }
                const int zone_count = data["zone_count"].get<int>();
                if (zone_count != static_cast<int>(levels.size())) {
                    cerr << "❌ JSON 오류: zone_count(" << zone_count
                         << ") != levels.size(" << levels.size() << ")" << endl;
                    return;
                }
            }

            if (lag_sec > 30) {
                cerr << "⚠️ 지연 데이터 감지: " << lag_sec << "초 전 update_time" << endl;
            }

            if (send_congestion_packet(uart_fd_, levels)) {
                cout << "STM 전송 완료 levels.size=" << levels.size() << endl;
            }
        } catch (const exception& e) {
            cerr << "❌ JSON 파싱 실패: " << e.what() << endl;
        }
    }

    void connection_lost(const string& cause) override {
        cerr << "⚠️ MQTT 연결 끊김: " << cause << endl;
    }

private:
    int uart_fd_;
};

static void handle_signal(int) {
    g_running = 0;
}

int main() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    const AppConfig config = load_config("config.json");
    const string broker_uri = "tcp://" + config.broker_ip + ":" + to_string(config.broker_port);

    int uart_fd = init_uart(config.uart_device.c_str());
    if (uart_fd < 0) return 1;

    mqtt::async_client client(broker_uri, config.client_id);
    DisplayMqttCallback cb(uart_fd);
    client.set_callback(cb);

    mqtt::connect_options opts;
    opts.set_keep_alive_interval(20);
    opts.set_clean_session(true);
    opts.set_automatic_reconnect(true);

    try {
        cout << "🛠️ MQTT broker: " << broker_uri << ", topic: " << TOPIC_CONGESTION << endl;
        client.connect(opts)->wait();
        cout << "✅ MQTT 연결 완료" << endl;
        client.subscribe(TOPIC_CONGESTION, 1)->wait();
        cout << "📡 구독 시작: " << TOPIC_CONGESTION << endl;

        while (g_running) {
            this_thread::sleep_for(chrono::milliseconds(200));
        }

        client.disconnect()->wait();
    } catch (const mqtt::exception& e) {
        cerr << "❌ MQTT 오류: " << e.what() << endl;
        close(uart_fd);
        return 1;
    }

    close(uart_fd);
    return 0;
}
