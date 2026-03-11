#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

using namespace std;

static constexpr uint8_t PKT_STX = 0xAA;
static constexpr uint8_t PKT_ETX = 0x55;
static constexpr uint8_t CMD_PLAY_WAV = 0x20;
static constexpr uint8_t CMD_GET_WAVS = 0x21;

uint8_t calc_crc(uint8_t cmd, uint8_t len, uint8_t *data)
{
    uint8_t crc = cmd ^ len;
    if (0 < len)
    {
        for (int i = 0; i < len; i++)
            crc ^= data[i];
    }
    crc ^= PKT_ETX;

    return crc;
}

int init_uart(const char *device)
{
    int fd = open(device, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
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

    //
    options.c_cc[VMIN] = 0;   // 최소 0바이트여도 리턴 가능
    options.c_cc[VTIME] = 10; // 1.0초(단위: 0.1초)
    //

    tcsetattr(fd, TCSANOW, &options);
    tcflush(fd, TCIOFLUSH);

    cout << "UART 초기화 완료: " << device << " (115200 baud)" << endl;
    return fd;
}

bool read_packet(int fd, vector<uint8_t> &out, int timeout_ms = 2000)
{
    out.clear();
    auto t0 = chrono::steady_clock::now();

    bool in_frame = false;
    uint8_t b;

    while (true)
    {
        // 전체 타임아웃
        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - t0).count();
        if (elapsed > timeout_ms)
            return false;

        ssize_t n = read(fd, &b, 1);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            perror("read");
            return false;
        }
        if (n == 0)
        {
            // VTIME 타임아웃으로 0 리턴될 수 있음 -> 계속 대기
            continue;
        }

        if (!in_frame)
        {
            if (b == PKT_STX)
            {
                in_frame = true;
                out.push_back(b);
            }
            continue;
        }
        else
        {
            out.push_back(b);
            if (b == PKT_ETX)
            {
                return true; // 프레임 완성
            }
            // 너무 길어지면 리셋(안전장치)
            if (out.size() > 512)
            {
                out.clear();
                in_frame = false;
            }
        }
    }
}

static bool read_byte_timeout(int fd, uint8_t &b,
                              const chrono::steady_clock::time_point &t0,
                              int timeout_ms)
{
    while (true)
    {
        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - t0).count();
        if (elapsed > timeout_ms)
            return false;

        ssize_t n = read(fd, &b, 1);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            perror("read");
            return false;
        }
        if (n == 0)
        {
            // termios VTIME 타임아웃으로 0 리턴될 수 있음 -> 계속 대기
            continue;
        }
        return true;
    }
}

// out에는 [STX][CMD][LEN][DATA...][CRC][ETX] 전체 프레임을 담아줌
bool read_packet_len_based(int fd, vector<uint8_t> &out, int timeout_ms = 2000)
{
    out.clear();
    auto t0 = chrono::steady_clock::now();

    // 1) STX 찾기
    uint8_t b = 0;
    while (true)
    {
        if (!read_byte_timeout(fd, b, t0, timeout_ms))
            return false;
        if (b == PKT_STX)
            break;
    }

    // 2) CMD, LEN 읽기
    uint8_t cmd = 0, len = 0;
    if (!read_byte_timeout(fd, cmd, t0, timeout_ms))
        return false;
    if (!read_byte_timeout(fd, len, t0, timeout_ms))
        return false;

    // 3) DATA len 바이트 읽기
    vector<uint8_t> data;
    data.resize(len);
    for (int i = 0; i < len; i++)
    {
        if (!read_byte_timeout(fd, data[i], t0, timeout_ms))
            return false;
    }

    // 4) CRC, ETX 읽기
    uint8_t crc_rx = 0, etx = 0;
    if (!read_byte_timeout(fd, crc_rx, t0, timeout_ms))
        return false;
    if (!read_byte_timeout(fd, etx, t0, timeout_ms))
        return false;

    if (etx != PKT_ETX)
    {
        cerr << "ETX mismatch: " << hex << (int)etx << dec << "\n";
        return false;
    }

    // 5) CRC 검증
    uint8_t crc_exp = calc_crc(cmd, len, (len ? data.data() : nullptr));
    if (crc_rx != crc_exp)
    {
        cerr << "CRC mismatch: recv=" << hex << (int)crc_rx
             << " exp=" << (int)crc_exp << dec << "\n";
        return false;
    }

    // 6) 프레임 구성
    out.reserve(1 + 1 + 1 + len + 1 + 1);
    out.push_back(PKT_STX);
    out.push_back(cmd);
    out.push_back(len);
    out.insert(out.end(), data.begin(), data.end());
    out.push_back(crc_rx);
    out.push_back(PKT_ETX);

    return true;
}

vector<string> parse_file_list_from_frame(const vector<uint8_t> &frame)
{
    // frame: [STX][CMD][LEN][DATA...][CRC][ETX]
    uint8_t len = frame[2];
    const uint8_t *data = (len ? &frame[3] : nullptr);

    string s((const char *)data, (size_t)len);

    vector<string> files;
    string cur;

    for (char c : s)
    {
        if (c == '\n')
        {
            if (!cur.empty())
                files.push_back(cur);
            cur.clear();
        }
        else if (c != '\r' && c != '\0')
        {
            cur.push_back(c);
        }
    }
    if (!cur.empty())
        files.push_back(cur);

    return files;
}

vector<string> send_get_wavs(int uart_fd)
{
    // CMD: GET WAVES
    uint8_t packet[5];
    packet[0] = PKT_STX;
    packet[1] = CMD_GET_WAVS;
    packet[2] = 0x00;
    packet[3] = calc_crc(CMD_GET_WAVS, 0, 0);
    packet[4] = PKT_ETX;

    write(uart_fd, packet, sizeof(packet));
    tcdrain(uart_fd);

    cout << "[SEND] GET_WAVES" << endl;
    //

    // CMD: RESP WAVES
    vector<uint8_t> rx;
    vector<string> files;
    // read_packet
    if (read_packet_len_based(uart_fd, rx, 5000))
    {
        printf("[RECV] %zu bytes: ", rx.size());
        for (auto v : rx)
            printf("%02X ", v);
        printf("\n");

        files = parse_file_list_from_frame(rx);
        cout << "files(" << files.size() << ")\n";
        for (auto &f : files)
            cout << " - " << f << "\n";

        return files;
    }
    else
    {
        cout << "[RECV] timeout\n";
    }

    return files;
}

void send_play_wav(int uart_fd, const string &filename)
{
    // 프레임: [STX][CMD][LEN][DATA...][CRC][ETX]
    if (filename.size() > 255)
    {
        std::cerr << "filename too long\n";
        return;
    }

    std::vector<uint8_t> pkt;
    uint8_t len = (uint8_t)filename.size();

    pkt.reserve(1 + 1 + 1 + len + 1 + 1);
    pkt.push_back(PKT_STX);
    pkt.push_back(CMD_PLAY_WAV);
    pkt.push_back(len);

    for (char c : filename)
        pkt.push_back((uint8_t)c);

    uint8_t crc = calc_crc(CMD_PLAY_WAV, len, (len ? (uint8_t *)pkt.data() + 3 : nullptr));
    pkt.push_back(crc);
    pkt.push_back(PKT_ETX);

    write(uart_fd, pkt.data(), pkt.size());
    tcdrain(uart_fd);

    std::cout << "[SEND] PLAY_WAVE: " << filename << "\n";
}

int main()
{
    int fd = init_uart("/dev/ttyS0");
    if (fd < 0)
        return 1;

    auto list = send_get_wavs(fd); // .wav 파일 목록 불러오기
    send_play_wav(fd, list[2]);    // e.g. 3번째 파일 선택

    close(fd);
    cout << "종료" << endl;
    return 0;
}
