#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

using namespace std;

static constexpr uint8_t PKT_STX            = 0xAA;
static constexpr uint8_t PKT_ETX            = 0x55;
static constexpr uint8_t CMD_BULK_CONGESTION = 0x10;

uint8_t calc_crc(uint8_t cmd, uint8_t len, uint8_t* data) {
    uint8_t crc = cmd ^ len;
    for (int i = 0; i < len; i++) crc ^= data[i];

//    crc ^= PKT_ETX;

    return crc;
}

int init_uart(const char* device) {
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

void start_sequential_test_10s(int uart_fd) {
    cout << "🚀 순차 이동 테스트 시작 (10초 간격, 총 8회)" << endl;

    for (int step = 0; step < 8; ++step) {
        uint8_t levels[8];
        for (int i = 0; i < 8; ++i) levels[i] = 0x02;
        levels[step % 8]       = 0x00;
        levels[(step + 1) % 8] = 0x01;

        uint8_t packet[13];
        packet[0] = PKT_STX;
        packet[1] = CMD_BULK_CONGESTION;
        packet[2] = 0x08;
        for (int i = 0; i < 8; ++i) packet[3 + i] = levels[i];
        packet[11] = calc_crc(CMD_BULK_CONGESTION, 8, levels);
        packet[12] = PKT_ETX;

        write(uart_fd, packet, sizeof(packet));
        tcdrain(uart_fd); // 데이터가 물리적으로 모두 전송될 때까지 대기한다.

        printf("[Step %d/8] 전송 데이터: ", step + 1);
        for (int i = 0; i < 8; i++) printf("%d ", levels[i]);
        printf("\n");

        if (step < 7) {
            cout << "⏳ 10초 대기 중..." << endl;
            this_thread::sleep_for(chrono::seconds(10));
        }
    }
    cout << "✅ 순차 테스트 완료!" << endl;
}

int main() {
    int fd = init_uart("/dev/ttyS0");
    if (fd < 0) return 1;

    start_sequential_test_10s(fd);

    close(fd);
    return 0;
}
