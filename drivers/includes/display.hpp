#ifndef DISPLAY_HPP
#define DISPLAY_HPP

/**
 * @brief 디스플레이 MQTT 구독을 초기화합니다.
 *        config.json의 congestion_topic 토픽을 구독하여
 *        서버로부터 혼잡도 데이터를 수신하고 STM32로 전달합니다.
 * @param uart_fd STM32로의 UART 파일 디스크립터
 */
void init_display(int uart_fd);

#endif // DISPLAY_HPP
