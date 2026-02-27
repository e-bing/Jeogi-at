// qt_comm.h
#ifndef QT_COMM_H
#define QT_COMM_H

#include <arpa/inet.h>
#include <fcntl.h>
#include <mariadb/mysql.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <unistd.h>

#include <chrono>
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

// 공통 하이브리드 패킷 전송 함수
bool send_camera_packet(SSL* ssl, uint32_t cam_id, const string& json_str,
                        const vector<unsigned char>& img_data);

// 영상 전송 전담 스레드 함수
void video_streaming_worker(SSL* ssl, bool* client_connected);

#endif  // QT_COMM_H
