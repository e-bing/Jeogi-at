#include "../includes/tls_server.hpp"

SSL_CTX* g_ssl_ctx = nullptr;

void init_tls() {
  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();

  g_ssl_ctx = SSL_CTX_new(TLS_server_method());
  if (!g_ssl_ctx) {
    std::cerr << "SSL_CTX_new 실패" << std::endl;
    ERR_print_errors_fp(stderr);
    exit(1);
  }

  if (SSL_CTX_use_certificate_file(g_ssl_ctx, "../config/server.crt",
                                   SSL_FILETYPE_PEM) <= 0) {
    std::cerr << "인증서 로드 실패: server.crt" << std::endl;
    ERR_print_errors_fp(stderr);
    exit(1);
  }

  if (SSL_CTX_use_PrivateKey_file(g_ssl_ctx, "../config/server.key",
                                  SSL_FILETYPE_PEM) <= 0) {
    std::cerr << "개인키 로드 실패: server.key" << std::endl;
    ERR_print_errors_fp(stderr);
    exit(1);
  }

  if (!SSL_CTX_check_private_key(g_ssl_ctx)) {
    std::cerr << "개인키와 인증서가 일치하지 않음" << std::endl;
    ERR_print_errors_fp(stderr);
    exit(1);
  }

  std::cout << "✅ TLS 초기화 완료 (TLS 1.2/1.3 지원)" << std::endl;
}

void cleanup_tls() {
  if (g_ssl_ctx) {
    SSL_CTX_free(g_ssl_ctx);
    g_ssl_ctx = nullptr;
  }
}

bool kill_process_using_port(int port) {
  std::string cmd = "lsof -ti:" + std::to_string(port) + " 2>/dev/null";
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) return true;

  char buffer[128];
  std::vector<std::string> pids;

  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    std::string pid = buffer;
    pid.erase(pid.find_last_not_of(" \n\r\t") + 1);
    if (!pid.empty()) {
      pids.push_back(pid);
    }
  }
  pclose(pipe);

  if (pids.empty()) return true;

  for (const auto& pid : pids) {
    std::string kill_cmd = "kill -9 " + pid + " 2>/dev/null";
    system(kill_cmd.c_str());
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  return true;
}