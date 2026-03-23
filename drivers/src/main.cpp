#include "motor.hpp"
#include "sensor.hpp"
#include "sht20.hpp"
#include "system_monitor.hpp"
#include "communication.hpp"
#include "audio.hpp"
#include "display.hpp"
#include "../../protocol/message_types.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>

using json = nlohmann::json;
using namespace std;

// STM32 connected UART fd shared by sensor/audio/display modules.
int g_uart_fd = -1;
int g_sht20_fd = -1;

namespace
{
    constexpr uint16_t STREAM_SYNC_WORD = 0xAA55;
    constexpr int STREAM_TCP_PORT = 5000;
    constexpr uint16_t STREAM_FRAME_BYTES = 1920;

    constexpr const char *STREAM_SPI_DEV = "/dev/spidev0.0";
    constexpr uint32_t STREAM_SPI_SPEED_HZ = 2000000;
    constexpr uint8_t STREAM_SPI_BITS_PER_WORD = 8;
    constexpr uint8_t STREAM_SPI_MODE_VALUE = SPI_MODE_0;

    constexpr bool ENABLE_STREAM_FILE_SAVE = true;
    constexpr const char *STREAM_OUTPUT_PCM_FILE = "received_48k_mono.pcm";
}

#pragma pack(push, 1)
struct AudioPacketHeader
{
    uint16_t sync;
    uint16_t payloadSize;
    uint32_t seq;
};
#pragma pack(pop)

ssize_t recv_exact(int sockfd, void *buffer, size_t size)
{
    size_t total_read = 0;
    auto *out = static_cast<uint8_t *>(buffer);

    while (total_read < size)
    {
        ssize_t n = recv(sockfd, out + total_read, size - total_read, 0);
        if (n == 0)
        {
            return 0;
        }
        if (n < 0)
        {
            return -1;
        }
        total_read += static_cast<size_t>(n);
    }

    return static_cast<ssize_t>(total_read);
}

bool create_stream_server_socket(int &listen_fd)
{
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
    {
        cerr << "[AudioStream] socket() failed" << endl;
        return false;
    }

    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        cerr << "[AudioStream] setsockopt() failed" << endl;
        close(listen_fd);
        listen_fd = -1;
        return false;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(STREAM_TCP_PORT);

    if (bind(listen_fd, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) < 0)
    {
        cerr << "[AudioStream] bind() failed on port " << STREAM_TCP_PORT << endl;
        close(listen_fd);
        listen_fd = -1;
        return false;
    }

    if (listen(listen_fd, 1) < 0)
    {
        cerr << "[AudioStream] listen() failed" << endl;
        close(listen_fd);
        listen_fd = -1;
        return false;
    }

    return true;
}

bool open_stream_spi(int &spi_fd)
{
    spi_fd = open(STREAM_SPI_DEV, O_RDWR);
    if (spi_fd < 0)
    {
        cerr << "[AudioStream] open(" << STREAM_SPI_DEV << ") failed" << endl;
        return false;
    }

    uint8_t mode = STREAM_SPI_MODE_VALUE;
    uint8_t bits = STREAM_SPI_BITS_PER_WORD;
    uint32_t speed = STREAM_SPI_SPEED_HZ;

    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0)
    {
        cerr << "[AudioStream] SPI_IOC_WR_MODE failed" << endl;
        close(spi_fd);
        spi_fd = -1;
        return false;
    }

    if (ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0)
    {
        cerr << "[AudioStream] SPI_IOC_WR_BITS_PER_WORD failed" << endl;
        close(spi_fd);
        spi_fd = -1;
        return false;
    }

    if (ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0)
    {
        cerr << "[AudioStream] SPI_IOC_WR_MAX_SPEED_HZ failed" << endl;
        close(spi_fd);
        spi_fd = -1;
        return false;
    }

    return true;
}

bool send_stream_frame_over_spi(int spi_fd, const uint8_t *data, size_t len)
{
    vector<uint8_t> rx_dummy(len, 0);

    spi_ioc_transfer transfer{};
    transfer.tx_buf = reinterpret_cast<unsigned long>(data);
    transfer.rx_buf = reinterpret_cast<unsigned long>(rx_dummy.data());
    transfer.len = static_cast<uint32_t>(len);
    transfer.speed_hz = STREAM_SPI_SPEED_HZ;
    transfer.bits_per_word = STREAM_SPI_BITS_PER_WORD;

    int ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &transfer);
    if (ret < 1)
    {
        cerr << "[AudioStream] SPI_IOC_MESSAGE failed" << endl;
        return false;
    }

    return true;
}

void handle_audio_stream_client(int client_fd, int spi_fd, ofstream *pcm_out)
{
    vector<uint8_t> payload(STREAM_FRAME_BYTES);

    while (true)
    {
        AudioPacketHeader header{};
        ssize_t header_read = recv_exact(client_fd, &header, sizeof(header));
        if (header_read == 0)
        {
            cout << "[AudioStream] client disconnected" << endl;
            return;
        }
        if (header_read < 0)
        {
            cerr << "[AudioStream] failed to receive header" << endl;
            return;
        }

        uint16_t sync = header.sync;
        uint16_t payload_size = header.payloadSize;
        uint32_t seq = header.seq;

        if (sync != STREAM_SYNC_WORD)
        {
            cerr << "[AudioStream] invalid sync: 0x" << hex << sync << dec << endl;
            return;
        }

        if (payload_size != STREAM_FRAME_BYTES)
        {
            cerr << "[AudioStream] invalid payload size: " << payload_size << endl;
            return;
        }

        ssize_t payload_read = recv_exact(client_fd, payload.data(), payload_size);
        if (payload_read <= 0)
        {
            cerr << "[AudioStream] failed to receive payload" << endl;
            return;
        }

        if (pcm_out != nullptr)
        {
            pcm_out->write(reinterpret_cast<const char *>(payload.data()),
                           static_cast<streamsize>(payload_size));
            if (!pcm_out->good())
            {
                cerr << "[AudioStream] pcm file write failed" << endl;
                return;
            }
        }

        if (!send_stream_frame_over_spi(spi_fd, payload.data(), payload_size))
        {
            cerr << "[AudioStream] SPI send failed, seq=" << seq << endl;
            return;
        }

        cout << "[AudioStream] seq=" << seq << ", payload=" << payload_size << endl;
    }
}

void run_audio_stream_server()
{
    int listen_fd = -1;
    int spi_fd = -1;

    if (!create_stream_server_socket(listen_fd))
    {
        return;
    }

    if (!open_stream_spi(spi_fd))
    {
        close(listen_fd);
        return;
    }

    ofstream pcm_out;
    if (ENABLE_STREAM_FILE_SAVE)
    {
        pcm_out.open(STREAM_OUTPUT_PCM_FILE, ios::binary);
        if (!pcm_out.is_open())
        {
            cerr << "[AudioStream] failed to open " << STREAM_OUTPUT_PCM_FILE << endl;
            close(spi_fd);
            close(listen_fd);
            return;
        }
    }

    cout << "[AudioStream] listening on 0.0.0.0:" << STREAM_TCP_PORT << endl;
    cout << "[AudioStream] forwarding to " << STREAM_SPI_DEV << endl;

    while (true)
    {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, reinterpret_cast<sockaddr *>(&client_addr), &client_len);
        if (client_fd < 0)
        {
            cerr << "[AudioStream] accept() failed" << endl;
            continue;
        }

        char client_ip[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        cout << "[AudioStream] client connected: " << client_ip
             << ":" << ntohs(client_addr.sin_port) << endl;

        handle_audio_stream_client(client_fd, spi_fd, ENABLE_STREAM_FILE_SAVE ? &pcm_out : nullptr);
        close(client_fd);
    }
}

void shutdown_and_exit(int)
{
    close_uart(g_uart_fd);
    close_sht20(g_sht20_fd);
    _Exit(0);
}

int main()
{
    cout << "========================================" << endl;
    cout << "   Motor Pi - Start" << endl;
    cout << "========================================" << endl;

    init_comm_mqtt();
    init_motor_mqtt();

    g_uart_fd = init_comm_uart("/dev/ttyS0");
    if (g_uart_fd >= 0)
    {
        thread([]()
               { receive_sensor_data(g_uart_fd); })
            .detach();
    }
    else
    {
        cerr << "UART initialization failed - sensor receive disabled" << endl;
    }

    g_sht20_fd = init_sht20();
    if (g_sht20_fd >= 0)
    {
        thread([]()
               { run_sht20_monitor(g_sht20_fd); })
            .detach();
        cout << "SHT20 monitoring started" << endl;
    }
    else
    {
        cerr << "SHT20 initialization failed" << endl;
    }

    if (g_uart_fd >= 0)
    {
        init_audio(g_uart_fd);
        init_audio_mqtt();
    }

    thread(run_audio_stream_server).detach();

    if (g_uart_fd >= 0)
    {
        init_display(g_uart_fd);
    }

    thread([]()
           {
               while (true)
               {
                   DeviceStats stats = get_server_stats();
                   json payload = {
                       {Protocol::FIELD_DEVICE, "firmware"},
                       {Protocol::FIELD_CPU_USAGE, stats.cpu_usage},
                       {Protocol::FIELD_CPU_TEMP, stats.cpu_temp},
                       {Protocol::FIELD_DISK_USAGE, stats.disk_usage}};
                   publish_to_server(Protocol::MQTT_TOPIC_SYSTEM_FIRMWARE, payload.dump());
                   cout << "[System] stats sent: " << payload.dump() << endl;
                   this_thread::sleep_for(chrono::seconds(5));
               } })
        .detach();

    signal(SIGINT, shutdown_and_exit);
    signal(SIGTERM, shutdown_and_exit);

    cout << "All services are running" << endl;

    while (true)
    {
        this_thread::sleep_for(chrono::hours(24));
    }

    return 0;
}
