#ifndef SENSOR_H
#define SENSOR_H

extern int g_uart_fd;

/**
 * @brief STM32와의 UART 연결을 초기화합니다.
 * @param device UART 장치 경로 (예: "/dev/ttyS0")
 * @return 성공 시 파일 디스크립터, 실패 시 -1
 */
int init_uart(const char* device);

/**
 * @brief UART 연결을 종료합니다.
 * @param uart_fd 닫을 파일 디스크립터
 */
void close_uart(int uart_fd);

/**
 * @brief UART를 통해 STM32로부터 센서 데이터를 수신합니다.
 *        자동 모드 시 센서값으로 모터를 제어하고,
 *        항상 서버로 센서값을 MQTT로 전송합니다.
 * @param uart_fd UART 장치 파일 디스크립터
 */
void receive_sensor_data(int uart_fd);

#endif // SENSOR_H
