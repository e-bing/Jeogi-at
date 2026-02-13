#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mariadb/mysql.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <cstring>
#include <fstream>
#include <fcntl.h>      // ⭐ UART용
#include <termios.h>    // ⭐ UART용

using json = nlohmann::json;
using namespace std;

struct DBConfig {
    string host = "localhost";
    string user = "iam";
    string pass = "";
    string db = "test";
};

SSL_CTX *g_ssl_ctx = nullptr;

// ⭐ UART 초기화 함수 (STM32 연결용)
int init_uart(const char* device) {
    int uart_fd = open(device, O_RDWR | O_NOCTTY);
    if (uart_fd < 0) {
        cerr << "UART 열기 실패: " << device << endl;
        return -1;
    }

    struct termios options;
    tcgetattr(uart_fd, &options);

    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);

    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;

    tcsetattr(uart_fd, TCSANOW, &options);
    tcflush(uart_fd, TCIOFLUSH);

    cout << "✅ UART 초기화 완료: " << device << " (115200 baud)" << endl;
    return uart_fd;
}

// ⭐ STM32에서 MQ-135(CO2) + MQ-7(CO) 데이터 수신 및 DB 저장
void receive_sensor_data(int uart_fd, MYSQL* conn) {
    char buffer[256];
    string line_buffer = "";

    while (true) {
        int bytes = read(uart_fd, buffer, sizeof(buffer) - 1);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            line_buffer += string(buffer);

            size_t pos;
            while ((pos = line_buffer.find('\n')) != string::npos) {
                string line = line_buffer.substr(0, pos);
                line_buffer.erase(0, pos + 1);

                if (line.empty() || line == "\r") continue;


                // JSON 파싱
                try {
                    json data = json::parse(line);
                    
                    // 새로운 JSON 형식: {"sensors":[{"type":"MQ135","co2":123.45},{"type":"MQ7","co":67.89}]}
                    if (data.contains("sensors") && data["sensors"].is_array()) {
                        float co2_value = 0.0;
                        float co_value = 0.0;

                        // 센서 배열 순회
                        for (const auto& sensor : data["sensors"]) {
                            string type = sensor["type"];
                            
                            if (type == "MQ135" && sensor.contains("co2")) {
                                co2_value = sensor["co2"];
                            }
                            else if (type == "MQ7" && sensor.contains("co")) {
                                co_value = sensor["co"];
                            }
                        }

                        // ⭐ air_stats 테이블에 저장
                        // co_level: MQ-7의 CO 값
                        // toxic_gas_level: MQ-135의 CO2 값
                        string sql = "INSERT INTO air_stats (station_id, co_level, toxic_gas_level, fire_detected, recorded_at) "
                                     "VALUES (1, " + to_string(co_value) + ", " + to_string(co2_value) + ", 0, NOW())";

                        if (mysql_query(conn, sql.c_str())) {
                            cerr << "❌ DB 저장 실패: " << mysql_error(conn) << endl;
                        } else {
                            cout << "✅ DB 저장 완료: CO = " << co_value << " ppm, CO2 = " << co2_value << " ppm" << endl << endl;
                        }
                    }
                    // 구버전 호환 (단일 센서 형식)
                    else if (data.contains("sensor") && data.contains("co2")) {
                        float co2 = data["co2"];
                        cout << "📊 수신 (구버전): " << data["sensor"] << " = " << co2 << " ppm" << endl;

                        string sql = "INSERT INTO air_stats (station_id, co_level, toxic_gas_level, fire_detected, recorded_at) "
                                     "VALUES (1, 0.0, " + to_string(co2) + ", 0, NOW())";

                        if (mysql_query(conn, sql.c_str())) {
                            cerr << "❌ DB 저장 실패: " << mysql_error(conn) << endl;
                        } else {
                            cout << "✅ DB 저장 완료: CO2 = " << co2 << " ppm (구버전)" << endl;
                        }
                    }

                } catch (json::exception& e) {
                    // JSON 아닌 라인 무시 (디버깅 메시지 등)
                    cerr << "JSON 파싱 에러: " << e.what() << " (원문 확인 필요)" << endl;
                }
            }
        }
        this_thread::sleep_for(chrono::milliseconds(100));
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

    if (SSL_CTX_use_certificate_file(g_ssl_ctx, "server.crt", SSL_FILETYPE_PEM) <= 0) {
        cerr << "인증서 로드 실패: server.crt" << endl;
        ERR_print_errors_fp(stderr);
        exit(1);
    }

    if (SSL_CTX_use_PrivateKey_file(g_ssl_ctx, "server.key", SSL_FILETYPE_PEM) <= 0) {
        cerr << "개인키 로드 실패: server.key" << endl;
        ERR_print_errors_fp(stderr);
        exit(1);
    }

    if (!SSL_CTX_check_private_key(g_ssl_ctx)) {
        cerr << "개인키와 인증서가 일치하지 않음" << endl;
        ERR_print_errors_fp(stderr);
        exit(1);
    }

    cout << "TLS 초기화 완료 (TLS 1.2/1.3 지원)" << endl;
}

void cleanup_tls() {
    if (g_ssl_ctx) {
        SSL_CTX_free(g_ssl_ctx);
        g_ssl_ctx = nullptr;
    }
}

MYSQL* connect_db(DBConfig config) {
    MYSQL* conn = mysql_init(NULL);
    if (!mysql_real_connect(conn, config.host.c_str(), config.user.c_str(),
                            config.pass.c_str(), config.db.c_str(), 0, NULL, 0)) {
        cerr << "DB 연결 실패: " << mysql_error(conn) << endl;
        return nullptr;
    }
    return conn;
}

json get_realtime_congestion(MYSQL* conn) {
    string sql = R"(
        SELECT
            S.station_name,
            C.platform_no,
            C.passenger_count,
            C.status_msg
        FROM camera_stats C
        JOIN station_stats S ON C.station_id = S.station_id
        INNER JOIN (
            SELECT platform_no, MAX(recorded_at) as max_time
            FROM camera_stats
            GROUP BY platform_no
        ) Latest ON C.platform_no = Latest.platform_no AND C.recorded_at = Latest.max_time
        ORDER BY
            CAST(SUBSTRING_INDEX(C.platform_no, '-', 1) AS UNSIGNED) ASC,
            CAST(SUBSTRING_INDEX(C.platform_no, '-', -1) AS UNSIGNED) ASC;
    )";

    json result = json::array();

    if (mysql_query(conn, sql.c_str())) {
        cerr << "실시간 혼잡도 쿼리 실패: " << mysql_error(conn) << endl;
        return result;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    MYSQL_ROW row;

    while ((row = mysql_fetch_row(res))) {
        json item = {
            {"station", row[0] ? row[0] : "N/A"},
            {"platform", row[1] ? row[1] : "N/A"},
            {"count", row[2] ? stoi(row[2]) : 0},
            {"status", row[3] ? row[3] : "정상"}
        };
        result.push_back(item);
    }

    mysql_free_result(res);
    return result;
}

// ⭐ 실시간 공기질 (CO + CO2 모두 전송)
json get_realtime_air_quality(MYSQL* conn) {
    string sql = R"(
        SELECT
            station_id,
            co_level,
            toxic_gas_level,
            fire_detected,
            recorded_at
        FROM air_stats
        WHERE station_id = 1
        ORDER BY recorded_at DESC
        LIMIT 1;
    )";

    json result = json::array();

    if (mysql_query(conn, sql.c_str())) {
        cerr << "실시간 공기질 쿼리 실패: " << mysql_error(conn) << endl;
        return result;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    MYSQL_ROW row;

    if ((row = mysql_fetch_row(res))) {
        json item = {
            {"station", "Jeogi-Station"},
            {"co_level", row[1] ? stod(row[1]) : 0.0},       // ⭐ MQ-7의 CO
            {"co2_ppm", row[2] ? stod(row[2]) : 0.0},        // ⭐ MQ-135의 CO2 (toxic_gas_level)
            {"fire_detected", row[3] ? stoi(row[3]) == 1 : false},
            {"recorded_at", row[4] ? row[4] : "N/A"}
        };
        result.push_back(item);
    }

    mysql_free_result(res);
    return result;
}

// ⭐ 공기질 통계 (CO + CO2 모두 포함)
json get_air_quality_stats(MYSQL* conn, string cam_id) {
    string sql = R"(
        SELECT
            DAYOFWEEK(C.recorded_at) AS d_idx,
            HOUR(C.recorded_at) AS hour,
            AVG(A.co_level) AS avg_co,
            AVG(A.toxic_gas_level) AS avg_co2
        FROM camera_stats C
        JOIN air_stats A ON C.recorded_at = A.recorded_at AND C.station_id = A.station_id
        WHERE C.camera_id = ')" + cam_id + R"('
        GROUP BY d_idx, hour;
    )";

    json result = json::array();

    if (mysql_query(conn, sql.c_str())) {
        cerr << "공기질 쿼리 실패: " << mysql_error(conn) << endl;
        return result;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    MYSQL_ROW row;

    while ((row = mysql_fetch_row(res))) {
        json item = {
            {"day", row[0] ? stoi(row[0]) : 0},
            {"hour", row[1] ? stoi(row[1]) : 0},
            {"co", row[2] ? stod(row[2]) : 0.0},             // ⭐ MQ-7의 CO
            {"co2", row[3] ? stod(row[3]) : 0.0}             // ⭐ MQ-135의 CO2
        };
        result.push_back(item);
    }

    mysql_free_result(res);
    return result;
}

// ⭐ 승객 흐름 통계
json get_passenger_flow_stats(MYSQL* conn, string cam_id) {
    string sql = R"(
        SELECT
            DAYOFWEEK(recorded_at) AS d_idx,
            HOUR(recorded_at) AS hour,
            AVG(passenger_count) AS avg_p
        FROM camera_stats
        WHERE camera_id = ')" + cam_id + R"('
        GROUP BY d_idx, hour;
    )";

    json result = json::array();

    if (mysql_query(conn, sql.c_str())) {
        cerr << "승객 흐름 쿼리 실패: " << mysql_error(conn) << endl;
        return result;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    MYSQL_ROW row;

    while ((row = mysql_fetch_row(res))) {
        json item = {
            {"day", row[0] ? stoi(row[0]) : 0},
            {"hour", row[1] ? stoi(row[1]) : 0},
            {"avg_count", row[2] ? (int)stod(row[2]) : 0}
        };
        result.push_back(item);
    }

    mysql_free_result(res);
    return result;
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
        json msg1 = {
            {"type", "realtime"},
            {"title", "🚉 실시간 혼잡도"},
            {"data", realtime_data}
        };

        // 2. 실시간 공기질 (CO + CO2 모두 포함)
        json realtime_air = get_realtime_air_quality(conn);
        json msg2 = {
            {"type", "realtime_air"},
            {"title", "🌫️ 실시간 공기질"},
            {"data", realtime_air}
        };

        // 3. 공기질 통계 (CO + CO2)
        json air_data = get_air_quality_stats(conn, "CAM-01");
        json msg3 = {
            {"type", "air_stats"},
            {"camera", "CAM-01"},
            {"title", "📊 공기질 통계"},
            {"data", air_data}
        };

        // 4. 승객 흐름 통계
        json flow_data = get_passenger_flow_stats(conn, "CAM-01");
        json msg4 = {
            {"type", "flow_stats"},
            {"camera", "CAM-01"},
            {"title", "👥 승객 흐름 통계"},
            {"data", flow_data}
        };

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
    mysql_close(conn);
    close(client_socket);
    cout << "클라이언트 연결 종료" << endl;
}

int main() {
    const int PORT = 12345;

    init_tls();
    kill_process_using_port(PORT);

    // ⭐ STM32 UART 연결
    int uart_fd = init_uart("/dev/ttyS0");  // 또는 /dev/ttyUSB0, /dev/ttyAMA0
    if (uart_fd < 0) {
        cerr << "⚠️ UART 초기화 실패 - STM32 없이 계속 진행" << endl;
        uart_fd = -1;  // UART 없이도 서버 동작
    }

    // ⭐ DB 연결 (UART 수신용)
    DBConfig config;
    MYSQL* uart_conn = connect_db(config);
    if (!uart_conn) {
        cerr << "❌ DB 연결 실패" << endl;
        if (uart_fd >= 0) close(uart_fd);
        cleanup_tls();
        return 1;
    }

    // ⭐ UART 수신 스레드 시작 (UART 연결된 경우만)
    if (uart_fd >= 0) {
        thread uart_thread(receive_sensor_data, uart_fd, uart_conn);
        uart_thread.detach();
        cout << "🔌 STM32 UART 수신 스레드 시작 (MQ-135 + MQ-7)" << endl;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        cerr << "소켓 생성 실패" << endl;
        cleanup_tls();
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        cerr << "❌ 포트 " << PORT << " 바인드 실패: " << strerror(errno) << endl;
        cleanup_tls();
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 5) < 0) {
        cerr << "리스닝 실패" << endl;
        cleanup_tls();
        close(server_fd);
        return 1;
    }

    cout << "📡 TLS 서버 시작... Qt 클라이언트 연결 대기 중 (포트 " << PORT << ")" << endl;

    while (true) {
        int client_socket = accept(server_fd, NULL, NULL);
        if (client_socket < 0) {
            cerr << "accept 실패" << endl;
            continue;
        }
        cout << "새 클라이언트 연결 시도 → TLS 핸드셰이크 시작" << endl;
        thread(handle_client, client_socket).detach();
    }

    close(server_fd);
    if (uart_fd >= 0) close(uart_fd);
    mysql_close(uart_conn);
    cleanup_tls();
    return 0;
}
