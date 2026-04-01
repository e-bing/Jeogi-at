#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

namespace
{
    constexpr uint16_t SYNC_WORD = 0xAA55;
    constexpr int TCP_PORT = 5000;
    constexpr uint16_t FRAME_BYTES = 1920;

    constexpr const char *SPI_DEV = "/dev/spidev0.0";
    constexpr uint32_t SPI_SPEED_HZ = 2000000;
    constexpr uint8_t SPI_BITS_PER_WORD = 8;
    constexpr uint8_t SPI_MODE_VALUE = SPI_MODE_0;

    constexpr bool ENABLE_FILE_SAVE = true;
    constexpr const char *OUTPUT_PCM_FILE = "received_48k_mono.pcm";
}

#pragma pack(push, 1)
struct AudioPacketHeader
{
    uint16_t sync;
    uint16_t payloadSize;
    uint32_t seq;
};
#pragma pack(pop)

ssize_t recvExact(int sockfd, void *buffer, size_t size)
{
    size_t totalRead = 0;
    auto *out = static_cast<uint8_t *>(buffer);

    while (totalRead < size)
    {
        ssize_t n = recv(sockfd, out + totalRead, size - totalRead, 0);
        if (n == 0)
            return 0;
        if (n < 0)
            return -1;
        totalRead += static_cast<size_t>(n);
    }

    return static_cast<ssize_t>(totalRead);
}

bool createServerSocket(int &listenFd)
{
    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0)
    {
        std::cerr << "[ERROR] socket() failed\n";
        return false;
    }

    int opt = 1;
    if (setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        std::cerr << "[ERROR] setsockopt() failed\n";
        close(listenFd);
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(TCP_PORT);

    if (bind(listenFd, reinterpret_cast<sockaddr *>(&serverAddr), sizeof(serverAddr)) < 0)
    {
        std::cerr << "[ERROR] bind() failed\n";
        close(listenFd);
        return false;
    }

    if (listen(listenFd, 1) < 0)
    {
        std::cerr << "[ERROR] listen() failed\n";
        close(listenFd);
        return false;
    }

    return true;
}

bool openSpi(int &spiFd)
{
    spiFd = open(SPI_DEV, O_RDWR);
    if (spiFd < 0)
    {
        std::cerr << "[ERROR] open spi failed\n";
        return false;
    }

    uint8_t mode = SPI_MODE_VALUE;
    uint8_t bits = SPI_BITS_PER_WORD;
    uint32_t speed = SPI_SPEED_HZ;

    if (ioctl(spiFd, SPI_IOC_WR_MODE, &mode) < 0)
    {
        std::cerr << "[ERROR] SPI_IOC_WR_MODE failed\n";
        close(spiFd);
        return false;
    }

    if (ioctl(spiFd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0)
    {
        std::cerr << "[ERROR] SPI_IOC_WR_BITS_PER_WORD failed\n";
        close(spiFd);
        return false;
    }

    if (ioctl(spiFd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0)
    {
        std::cerr << "[ERROR] SPI_IOC_WR_MAX_SPEED_HZ failed\n";
        close(spiFd);
        return false;
    }

    return true;
}

bool spiSendFrame(int spiFd, const uint8_t *data, size_t len)
{
    std::vector<uint8_t> rxDummy(len, 0);

    spi_ioc_transfer tr{};
    tr.tx_buf = reinterpret_cast<unsigned long>(data);
    tr.rx_buf = reinterpret_cast<unsigned long>(rxDummy.data());
    tr.len = static_cast<uint32_t>(len);
    tr.speed_hz = SPI_SPEED_HZ;
    tr.bits_per_word = SPI_BITS_PER_WORD;
    tr.delay_usecs = 0;
    tr.cs_change = 0;

    int ret = ioctl(spiFd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 1)
    {
        std::cerr << "[ERROR] SPI_IOC_MESSAGE failed\n";
        return false;
    }

    return true;
}

int main()
{
    int listenFd = -1;
    int clientFd = -1;
    int spiFd = -1;

    if (!createServerSocket(listenFd))
        return 1;

    if (!openSpi(spiFd))
    {
        close(listenFd);
        return 1;
    }

    std::ofstream pcmOut;
    if (ENABLE_FILE_SAVE)
    {
        pcmOut.open(OUTPUT_PCM_FILE, std::ios::binary);
        if (!pcmOut.is_open())
        {
            std::cerr << "[ERROR] failed to open output file\n";
            close(spiFd);
            close(listenFd);
            return 1;
        }
    }

    std::cout << "[INFO] Listening on 0.0.0.0:" << TCP_PORT << '\n';
    std::cout << "[INFO] Waiting for client...\n";

    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);

    clientFd = accept(listenFd, reinterpret_cast<sockaddr *>(&clientAddr), &clientLen);
    if (clientFd < 0)
    {
        std::cerr << "[ERROR] accept() failed\n";
        if (pcmOut.is_open())
            pcmOut.close();
        close(spiFd);
        close(listenFd);
        return 1;
    }

    char clientIp[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));
    std::cout << "[INFO] Client connected: " << clientIp << ":" << ntohs(clientAddr.sin_port) << '\n';

    std::vector<uint8_t> payload(FRAME_BYTES);

    while (true)
    {
        AudioPacketHeader header{};
        ssize_t headerRead = recvExact(clientFd, &header, sizeof(header));

        if (headerRead == 0)
        {
            std::cout << "[INFO] Client disconnected\n";
            break;
        }
        if (headerRead < 0)
        {
            std::cerr << "[ERROR] failed to receive header\n";
            break;
        }

        // Qt에서 QDataStream으로 보냈다면 이 변환이 필요할 수 있음
        // uint16_t sync = ntohs(header.sync);
        // uint16_t payloadSize = ntohs(header.payloadSize);
        // uint32_t seq = ntohl(header.seq);

        uint16_t sync = header.sync;
        uint16_t payloadSize = header.payloadSize;
        uint32_t seq = header.seq;

        if (sync != SYNC_WORD)
        {
            std::cerr << "[ERROR] invalid sync: 0x" << std::hex << sync << std::dec << '\n';
            break;
        }

        if (payloadSize != FRAME_BYTES)
        {
            std::cerr << "[ERROR] invalid payload size: " << payloadSize << '\n';
            break;
        }

        ssize_t payloadRead = recvExact(clientFd, payload.data(), payloadSize);
        if (payloadRead <= 0)
        {
            std::cerr << "[ERROR] failed to receive payload\n";
            break;
        }

        if (ENABLE_FILE_SAVE)
        {
            pcmOut.write(reinterpret_cast<const char *>(payload.data()),
                         static_cast<std::streamsize>(payloadSize));
            if (!pcmOut.good())
            {
                std::cerr << "[ERROR] file write failed\n";
                break;
            }
        }

        if (!spiSendFrame(spiFd, payload.data(), payloadSize))
        {
            std::cerr << "[ERROR] SPI send failed, seq=" << seq << '\n';
            break;
        }

        std::cout << "[INFO] seq=" << seq
                  << ", payload=" << payloadSize
                  << " -> file"
                  << (ENABLE_FILE_SAVE ? " + spi" : " + spi only")
                  << '\n';
    }

    if (pcmOut.is_open())
        pcmOut.close();
    if (clientFd >= 0)
        close(clientFd);
    if (spiFd >= 0)
        close(spiFd);
    if (listenFd >= 0)
        close(listenFd);

    std::cout << "[INFO] Server terminated\n";
    return 0;
}