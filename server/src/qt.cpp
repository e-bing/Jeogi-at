// qt.cpp
#include "qt.h"

#include <openssl/err.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>

#include "database.h"

using json = nlohmann::json;
using namespace std;

SSL_CTX* g_ssl_ctx = nullptr;

void init_tls() {
  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();

  g_ssl_ctx = SSL_CTX_new(TLS_server_method());
  if (!g_ssl_ctx) {
    cerr << "SSL_CTX_new 실패" << endl;
    ERR_print_errors_fp(stderr);
    exit(1);
  }

  if (SSL_CTX_use_certificate_file(g_ssl_ctx, "../config/server.crt",
                                   SSL_FILETYPE_PEM) <= 0) {
    cerr << "인증서 로드 실패: server.crt" << endl;
    ERR_print_errors_fp(stderr);
    exit(1);
  }

  if (SSL_CTX_use_PrivateKey_file(g_ssl_ctx, "../config/server.key",
                                  SSL_FILETYPE_PEM) <= 0) {
    cerr << "개인키 로드 실패: server.key" << endl;
    ERR_print_errors_fp(stderr);
    exit(1);
  }

  if (!SSL_CTX_check_private_key(g_ssl_ctx)) {
    cerr << "개인키와 인증서가 일치하지 않음" << endl;
    ERR_print_errors_fp(stderr);
    exit(1);
  }

  cout << "✅ TLS 초기화 완료 (TLS 1.2/1.3 지원)" << endl;
}

void cleanup_tls() {
  if (g_ssl_ctx) {
    SSL_CTX_free(g_ssl_ctx);
    g_ssl_ctx = nullptr;
  }
}

bool kill_process_using_port(int port) {
  string cmd = "lsof -ti:" + to_string(port) + " 2>/dev/null";
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) return true;

  char buffer[128];
  vector<string> pids;

  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    string pid = buffer;
    pid.erase(pid.find_last_not_of(" \n\r\t") + 1);
    if (!pid.empty()) {
      pids.push_back(pid);
    }
  }
  pclose(pipe);

  if (pids.empty()) return true;

  for (const auto& pid : pids) {
    string kill_cmd = "kill -9 " + pid + " 2>/dev/null";
    system(kill_cmd.c_str());
  }
  this_thread::sleep_for(chrono::milliseconds(500));
  return true;
}

void handle_client(int client_socket) {
  SSL* ssl = SSL_new(g_ssl_ctx);
  if (!ssl) {
    cerr << "SSL_new 실패" << endl;
    close(client_socket);
    return;
  }

  SSL_set_fd(ssl, client_socket);

  if (SSL_accept(ssl) <= 0) {
    cerr << "TLS 핸드셰이크 실패" << endl;
    ERR_print_errors_fp(stderr);
    SSL_free(ssl);
    close(client_socket);
    return;
  }

  cout << "🔒 TLS 연결 성공 (Cipher: " << SSL_get_cipher(ssl) << ")" << endl;

  // Qt 클라이언트 전용 DB 연결
  DBConfig config;
  MYSQL* conn = connect_db(config);

  if (!conn) {
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_socket);
    return;
  }

  while (true) {
    // 1. 실시간 혼잡도
    json realtime_data = get_realtime_congestion(conn);
    json msg1 = {{"type", "realtime"},
                 {"title", "🚉 실시간 혼잡도"},
                 {"data", realtime_data}};

    // 2. 실시간 공기질 (CO + CO2)
    json realtime_air = get_realtime_air_quality(conn);
    json msg2 = {{"type", "realtime_air"},
                 {"title", "🌫️ 실시간 공기질"},
                 {"data", realtime_air}};

    // 3. 공기질 통계 (CO + CO2)
    json air_data = get_air_quality_stats(conn, "CAM-01");
    json msg3 = {{"type", "air_stats"},
                 {"camera", "CAM-01"},
                 {"title", "📊 공기질 통계"},
                 {"data", air_data}};

    // 4. 승객 흐름 통계
    json flow_data = get_passenger_flow_stats(conn, "CAM-01");
    json msg4 = {{"type", "flow_stats"},
                 {"camera", "CAM-01"},
                 {"title", "👥 승객 흐름 통계"},
                 {"data", flow_data}};

    string s1 = msg1.dump() + "\n";
    string s2 = msg2.dump() + "\n";
    string s3 = msg3.dump() + "\n";
    string s4 = msg4.dump() + "\n";

    if (SSL_write(ssl, s1.c_str(), s1.length()) <= 0) break;
    this_thread::sleep_for(chrono::milliseconds(100));

    if (SSL_write(ssl, s2.c_str(), s2.length()) <= 0) break;
    this_thread::sleep_for(chrono::milliseconds(100));

    if (SSL_write(ssl, s3.c_str(), s3.length()) <= 0) break;
    this_thread::sleep_for(chrono::milliseconds(100));

    if (SSL_write(ssl, s4.c_str(), s4.length()) <= 0) break;

    this_thread::sleep_for(chrono::seconds(1));
  }

  SSL_shutdown(ssl);
  SSL_free(ssl);
  close_db(conn);
  close(client_socket);
  cout << "클라이언트 연결 종료" << endl;
}
