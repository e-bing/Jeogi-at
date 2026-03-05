// tls_server.hpp
#ifndef TLS_SERVER_HPP
#define TLS_SERVER_HPP

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// TLS 컨텍스트 전역 객체 (다른 모듈에서 SSL_new 등에 사용)
extern SSL_CTX* g_ssl_ctx;

// server.crt / server.key를 로드하고 TLS 컨텍스트를 초기화합니다.
// 실패 시 즉시 exit(1)
void init_tls();

// TLS 컨텍스트를 해제합니다. 프로그램 종료 시 호출
void cleanup_tls();

// 특정 포트를 점유 중인 프로세스를 강제 종료합니다.
// 서버 재시작 시 포트 충돌 방지용
bool kill_process_using_port(int port);

#endif  // TLS_SERVER_HPP