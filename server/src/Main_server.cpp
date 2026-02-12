// main.cpp
#include "Database.h"
#include "Sensor.h"
#include "Qt.h"
#include <iostream>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

using namespace std;

int main() {
    const int PORT = 12345;

    cout << "==================================================" << endl;
    cout << "     Jeogi Station Monitoring Server" << endl;
    cout << "==================================================" << endl;

    // 1. TLS 초기화
    init_tls();

    // 2. 포트 충돌 해결
    kill_process_using_port(PORT);

    // 3. 센서 통신 스레드 시작 (별도 DB 연결)
    thread sensor_thread([]() {
        // UART 초기화
        int uart_fd = init_uart("/dev/ttyS0");
        if (uart_fd < 0) {
            cerr << "⚠️ UART 초기화 실패 - 센서 데이터 수신 불가" << endl;
            return;
        }

        // 센서용 DB 연결
        DBConfig config;
        MYSQL* sensor_conn = connect_db(config);
        if (!sensor_conn) {
            cerr << "❌ 센서용 DB 연결 실패" << endl;
            close_uart(uart_fd);
            return;
        }

        // 센서 데이터 수신 (무한 루프)
        receive_sensor_data(uart_fd, sensor_conn);

        // 정리 (여기까지 도달하지 않음)
        close_uart(uart_fd);
        close_db(sensor_conn);
    });
    sensor_thread.detach();  // 백그라운드에서 실행

    // 4. Qt 클라이언트 서버 소켓 생성
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
    cout << "==================================================" << endl;

    // 5. Qt 클라이언트 연결 처리 (메인 스레드)
    while (true) {
        int client_socket = accept(server_fd, NULL, NULL);
        if (client_socket < 0) {
            cerr << "accept 실패" << endl;
            continue;
        }

        cout << "새 클라이언트 연결 시도 → TLS 핸드셰이크 시작" << endl;

        // 각 클라이언트마다 별도 스레드 생성
        thread client_thread(handle_client, client_socket);
        client_thread.detach();
    }

    // 정리 (여기까지 도달하지 않음)
    close(server_fd);
    cleanup_tls();
    return 0;
}
