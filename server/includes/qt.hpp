// qt_comm.h
#ifndef QT_COMM_H
#define QT_COMM_H

#include <arpa/inet.h>
#include <fcntl.h>
#include <mariadb/mysql.h>
#include <netinet/tcp.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <thread>
#include <vector>

#include "../../protocol/camera_packet.hpp"
#include "congestion_analyzer.hpp"
#include "database.h"
#include "motor.h"
#include "shared_data.hpp"

struct SendPacket {
  enum class Type { JSON, CAMERA } type;
  std::vector<uint8_t> data;  // 전송할 원시 바이트
};

extern std::queue<SendPacket> g_send_queue;
extern std::mutex g_send_queue_mutex;
extern std::condition_variable g_send_cv;
extern std::atomic<bool> client_connected;

// TLS 초기화 및 정리
void init_tls();
void cleanup_tls();
extern SSL_CTX* g_ssl_ctx;

// Qt 클라이언트 핸들러 (스레드 함수)
void handle_client(int client_socket);

// 유틸리티
bool kill_process_using_port(int port);

// Qt에서 입력 받기
void handle_qt_command(const std::string& cmd_str);

// 영상 전송 전담 스레드 함수
void video_streaming_worker(std::atomic<bool>* client_connected,
                            std::queue<SendPacket>& q, std::mutex& mtx,
                            std::condition_variable& cv);

// 쓰기 전담 스레드 함수
void writer_thread_func(SSL* ssl, std::atomic<bool>* connected,
                        std::queue<SendPacket>& q, std::mutex& mtx,
                        std::condition_variable& cv, std::mutex& ssl_write_mtx);

// 카메라 패킷을 쓰기 큐에 넣는 함수
void enqueue_camera_packet(std::queue<SendPacket>& q, std::mutex& mtx,
                           std::condition_variable& cv, uint32_t cam_id,
                           const string& json_str,
                           const vector<unsigned char>& img_data);

// JSON 패킷을 쓰기 큐에 넣는 함수
void enqueue_json_packet(std::queue<SendPacket>& q, std::mutex& mtx,
                         std::condition_variable& cv, const string& json_str)

#endif  // QT_COMM_H
