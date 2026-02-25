// qt_comm.h
#ifndef QT_COMM_H
#define QT_COMM_H

#include <openssl/ssl.h>
#include <mariadb/mysql.h>
#include <string>

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

#endif // QT_COMM_H

