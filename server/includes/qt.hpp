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
#include <mutex>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "../../protocol/camera_packet.hpp"
#include "congestion_analyzer.hpp"
#include "database.hpp"
#include "motor.hpp"
#include "shared_data.hpp"
#include "system_monitor.hpp"

using std::string;
using std::vector;

struct SendPacket {
  enum class Type { JSON, CAMERA } type;
  std::vector<uint8_t> data;  // 전송할 원시 바이트
};

struct ClientSession {
  SSL* ssl;
  bool connected;
  std::queue<SendPacket> send_queue;
  std::mutex queue_mutex;
  std::condition_variable queue_cv;
};

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

// Qt 클라이언트로부터 명령을 수신하는 reader 스레드 함수
// SSL_read를 논블로킹으로 반복하며 '\n' 단위로 명령을 파싱
void reader_thread_func(SSL* ssl, std::atomic<bool>* connected);

// Qt 클라이언트로 패킷을 전송하는 writer 스레드 함수
// send_queue에서 패킷을 꺼내 SSL_write로 전송 (SSL_write 독점)
void writer_thread_func(SSL* ssl, std::atomic<bool>* connected,
                        std::queue<SendPacket>& q, std::mutex& mtx,
                        std::condition_variable& cv);

// 영상 프레임을 JPEG 인코딩하여 send_queue에 enqueue하는 스레드 함수
// Hanwha(ID:1, ~50fps), Pi 노드(ID:2~, 5fps)를 각각 처리
void video_streaming_worker(std::atomic<bool>* client_connected,
                            std::queue<SendPacket>& q, std::mutex& mtx,
                            std::condition_variable& cv);

// DEADBEEF 헤더를 포함한 카메라 패킷을 send_queue에 enqueue
// 큐가 6개 초과 시 드롭 (실시간성 유지)
void enqueue_camera_packet(std::queue<SendPacket>& q, std::mutex& mtx,
                           std::condition_variable& cv, uint32_t cam_id,
                           const string& json_str,
                           const vector<unsigned char>& img_data);

// DEADBEEF 헤더를 포함한 JSON 전용 패킷(camera_id=0)을 send_queue에 enqueue
// 큐가 10개 초과 시 드롭
void enqueue_json_packet(std::queue<SendPacket>& q, std::mutex& mtx,
                         std::condition_variable& cv, const string& json_str);

#endif  // QT_COMM_H
