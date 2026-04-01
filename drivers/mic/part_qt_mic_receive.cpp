#include <arpa/inet.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    constexpr uint16_t SYNC_WORD = 0xAA55;
    constexpr int PORT = 5000;
    constexpr uint16_t FRAME_BYTES = 1920;
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
    serverAddr.sin_port = htons(PORT);

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

int main()
{
    int listenFd = -1;
    if (!createServerSocket(listenFd))
    {
        return 1;
    }

    std::cout << "[INFO] Listening on 0.0.0.0:" << PORT << '\n';
    std::cout << "[INFO] Waiting for client...\n";

    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);

    int clientFd = accept(listenFd, reinterpret_cast<sockaddr *>(&clientAddr), &clientLen);
    if (clientFd < 0)
    {
        std::cerr << "[ERROR] accept() failed\n";
        close(listenFd);
        return 1;
    }

    char clientIp[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));
    std::cout << "[INFO] Client connected: " << clientIp << ":" << ntohs(clientAddr.sin_port) << '\n';

    std::ofstream pcmOut("received_48k_mono.pcm", std::ios::binary);
    if (!pcmOut.is_open())
    {
        std::cerr << "[ERROR] failed to open output file\n";
        close(clientFd);
        close(listenFd);
        return 1;
    }

    std::cout << "[INFO] File opened: received_48k_mono.pcm\n";

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

        std::cout << "[INFO] Header received: seq=" << header.seq
                  << ", payload=" << header.payloadSize << '\n';

        if (header.sync != SYNC_WORD)
        {
            std::cerr << "[ERROR] invalid sync: 0x" << std::hex << header.sync << std::dec << '\n';
            break;
        }

        if (header.payloadSize != FRAME_BYTES)
        {
            std::cerr << "[ERROR] invalid payload size: " << header.payloadSize << '\n';
            break;
        }

        std::vector<uint8_t> payload(header.payloadSize);
        ssize_t payloadRead = recvExact(clientFd, payload.data(), payload.size());
        if (payloadRead <= 0)
        {
            std::cerr << "[ERROR] failed to receive payload\n";
            break;
        }

        pcmOut.write(reinterpret_cast<const char *>(payload.data()),
                     static_cast<std::streamsize>(payload.size()));

        if (!pcmOut.good())
        {
            std::cerr << "[ERROR] file write failed\n";
            break;
        }

        pcmOut.flush();
    }

    pcmOut.close();
    close(clientFd);
    close(listenFd);

    std::cout << "[INFO] Server terminated\n";
    return 0;
}