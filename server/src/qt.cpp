// qt.cpp
#include "qt.h"
#include "database.h"
#include "motor.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <unistd.h>
#include <openssl/err.h>
#include <nlohmann/json.hpp>
#include <fcntl.h>

using json = nlohmann::json;
using namespace std;

extern int g_uart_fd;

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

    if (SSL_CTX_use_certificate_file(g_ssl_ctx, "../config/server.crt", SSL_FILETYPE_PEM) <= 0) {
        cerr << "인증서 로드 실패: server.crt" << endl;
        ERR_print_errors_fp(stderr);
        exit(1);
    }

    if (SSL_CTX_use_PrivateKey_file(g_ssl_ctx, "../config/server.key", SSL_FILETYPE_PEM) <= 0) {
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

// ✅ Qt 명령 처리 함수 (Motor.h의 함수 호출)
void handle_qt_command(const string& cmd_str) {
    try {
        json data = json::parse(cmd_str);
        string type = data.value("type", "");

        if (type == "device_command") {
            json cmdData = data.value("data", json::object());
            string device = cmdData.value("device", "");
            string action = cmdData.value("action", "");

            // 1️⃣ 모드 제어 (자동/수동 전환)
            if (device == "mode_control") {
                string uart_cmd = "";

                if (action == "auto") {
                    g_auto_mode = true;
                    cout << "🤖 [MODE] 자동 모드 활성화 (센서 기반 제어)" << endl;
                    uart_cmd = "{\"type\":\"mode_control\",\"action\":\"auto\"}\n";
                }
                else if (action == "manual") {
                    g_auto_mode = false;
                    cout << "👤 [MODE] 수동 모드 활성화 (Qt 제어)" << endl;
                    uart_cmd = "{\"type\":\"mode_control\",\"action\":\"manual\"}\n";
                }

                // ✅ STM32로 모드 변경 즉시 전송
                if (g_uart_fd >= 0 && !uart_cmd.empty()) {
                    write(g_uart_fd, uart_cmd.c_str(), uart_cmd.length());
                    cout << "📤 [UART] STM32 모드 변경 명령 전송 완료" << endl;
                }
                return;
            }

            // 2️⃣ 장치 제어 (수동 모드일 때만 동작)
            if (!g_auto_mode) {
                if (device == "motor") {
                    int speed = cmdData.value("speed", 100);
                    string motor_uart_cmd;

                    if (action == "start" || action == "on") {
                        cout << "🚀 [STATUS] MOTOR ON (Speed: " << speed << "%)" << endl;
                        // ✅ STM32가 인식할 수 있도록 JSON 끝에 \n 추가하여 직접 전송
                        motor_uart_cmd = "{\"type\":\"motor_control\",\"action\":\"start\",\"speed\":" + to_string(speed) + "}\n";
                    }
                    else if (action == "stop" || action == "off") {
                        cout << "🛑 [STATUS] MOTOR OFF" << endl;
                        motor_uart_cmd = "{\"type\":\"motor_control\",\"action\":\"stop\",\"speed\":0}\n";
                    }

                    if (g_uart_fd >= 0 && !motor_uart_cmd.empty()) {
                        write(g_uart_fd, motor_uart_cmd.c_str(), motor_uart_cmd.length());
                        cout << "📤 [UART] STM32 모터 명령 전송: " << motor_uart_cmd;
                    }
                }
                else if (device == "speaker") {
                    cout << "🔊 [STATUS] SPEAKER " << (action == "on" ? "ON" : "OFF") << endl;
                }
                else if (device == "lighting") {
                    cout << "💡 [STATUS] LIGHTING " << (action == "on" ? "ON" : "OFF") << endl;
                }
            } 
            else {
                // 자동 모드일 때 Qt에서 제어 명령이 온 경우
                cout << "⚠️ [AUTO MODE] Qt 수동 명령 무시됨 (현재 자동 모드 활성화 중)" << endl;
            }
        }
    } catch (json::exception& e) {
        cerr << "❌ Qt 명령 파싱 에러: " << e.what() << endl;
    }
}

void handle_client(int client_socket) {
    SSL *ssl = SSL_new(g_ssl_ctx);
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

    // 논블로킹 모드 설정 (명령 수신 대기 중 서버 멈춤 방지)
    int flags = fcntl(client_socket, F_GETFL, 0);
    fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);

    string cmd_buffer = "";
    
    while (true) {

	// ========== 1. Qt로부터 명령 수신 (논블로킹) ==========
        char rx_buffer[256];
        int bytes = SSL_read(ssl, rx_buffer, sizeof(rx_buffer) - 1);
        
        if (bytes > 0) {
            rx_buffer[bytes] = '\0';
            cmd_buffer += string(rx_buffer);
            
            size_t pos;
            while ((pos = cmd_buffer.find('\n')) != string::npos) {
                string cmd_line = cmd_buffer.substr(0, pos);
                cmd_buffer.erase(0, pos + 1);
                
                if (!cmd_line.empty() && cmd_line != "\r") {
                    handle_qt_command(cmd_line);
                }
            }
        }
        else if (bytes < 0) {
            int err = SSL_get_error(ssl, bytes);
            if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) break; 
        }
        else break; // 연결 종료

        // ========== 2. Qt로 실시간 데이터 전송 (DB 기반) ==========
        try {
            // 메시지들을 순차적으로 생성 및 전송
            json msg_realtime = {{"type", "realtime"}, {"title", "🚉 실시간 혼잡도"}, {"data", get_realtime_congestion(conn)}};
            json msg_air = {{"type", "realtime_air"}, {"title", "🌫️ 실시간 공기질"}, {"data", get_realtime_air_quality(conn)}};
            json msg_air_stats = {{"type", "air_stats"}, {"camera", "CAM-01"}, {"title", "📊 공기질 통계"}, {"data", get_air_quality_stats(conn, "CAM-01")}};
            json msg_flow = {{"type", "flow_stats"}, {"camera", "CAM-01"}, {"title", "👥 승객 흐름 통계"}, {"data", get_passenger_flow_stats(conn, "CAM-01")}};

            // 하나라도 전송 실패 시 루프 종료(disconnect)
            string s;
            s = msg_realtime.dump() + "\n";
            if (SSL_write(ssl, s.c_str(), s.length()) <= 0) break;
            
            s = msg_air.dump() + "\n";
            if (SSL_write(ssl, s.c_str(), s.length()) <= 0) break;

            this_thread::sleep_for(chrono::milliseconds(100)); // 클라이언트 수신 버퍼 고려

            s = msg_air_stats.dump() + "\n";
            if (SSL_write(ssl, s.c_str(), s.length()) <= 0) break;

            s = msg_flow.dump() + "\n";
            if (SSL_write(ssl, s.c_str(), s.length()) <= 0) break;

        } catch (const exception& e) {
            cerr << "데이터 전송 에러: " << e.what() << endl;
        }

        this_thread::sleep_for(chrono::seconds(1)); // 1초 대기 후 다음 갱신
    }


    SSL_shutdown(ssl);
    SSL_free(ssl);
    close_db(conn);
    close(client_socket);
    cout << "클라이언트 연결 종료" << endl;
}
