// sensor_comm.h
#ifndef SENSOR_COMM_H
#define SENSOR_COMM_H

#include <mariadb/mysql.h>
#include <string>

using namespace std;

// UART 초기화 및 관리
int init_uart(const char* device);
void close_uart(int uart_fd);

// 센서 데이터 수신 스레드 함수
void receive_sensor_data(int uart_fd, MYSQL* conn);

#endif // SENSOR_COMM_H
