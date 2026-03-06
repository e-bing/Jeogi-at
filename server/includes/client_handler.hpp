// client_handler.hpp
#ifndef CLIENT_HANDLER_HPP
#define CLIENT_HANDLER_HPP

#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <unistd.h>

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>
#include <queue>

#include "../../protocol/camera_packet.hpp"
#include "../../protocol/message_types.hpp"
#include "command_handler.hpp"
#include "congestion_analyzer.hpp"
#include "database.hpp"
#include "shared_data.hpp"
#include "system_monitor.hpp"
#include "tls_server.hpp"

// Qt 클라이언트로 전송할 패킷 단위
struct SendPacket {
  enum class Type { JSON, CAMERA } type;
  std::vector<uint8_t> data;  // DEADBEEF 헤더 + payload 원시 바이트
};

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

// 영상 프레임을 JPEG 인코딩하여 send_queue에 enqueue하는 스레드 함수
// Hanwha(ID:1, ~50fps), Pi 노드(ID:2~, 5fps)를 각각 처리
void video_streaming_worker(std::atomic<bool>* client_connected,
                            std::queue<SendPacket>& q, std::mutex& mtx,
                            std::condition_variable& cv);

// Qt 클라이언트로부터 명령을 수신하는 스레드 함수
// SSL_read를 논블로킹으로 반복하며 '\n' 단위로 명령을 파싱해
// handle_qt_command로 전달
void reader_thread_func(SSL* ssl, std::atomic<bool>* connected);

// Qt 클라이언트로 패킷을 전송하는 스레드 함수
// send_queue에서 패킷을 꺼내 SSL_write로 전송 (SSL 객체 독점 사용)
void writer_thread_func(SSL* ssl, std::atomic<bool>* connected,
                        std::queue<SendPacket>& q, std::mutex& mtx,
                        std::condition_variable& cv);

// TLS 핸드셰이크부터 연결 종료까지 Qt 클라이언트 수명주기 전체를 관리
// reader / writer / video 스레드를 생성하고 DB 데이터를 주기적으로 enqueue
void handle_client(int client_socket);

#endif  // CLIENT_HANDLER_HPP