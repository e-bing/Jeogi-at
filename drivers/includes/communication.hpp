#ifndef COMMUNICATION_HPP
#define COMMUNICATION_HPP

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

/* ─────────────────────────────────────────
   초기화
───────────────────────────────────────── */

/**
 * @brief MQTT 초기화. config.json에서 브로커 주소와 토픽을 읽어옵니다.
 *        프로그램 시작 시 한 번만 호출하세요.
 */
void init_comm_mqtt();

/**
 * @brief UART 초기화. 115200 baud, 8N1 설정.
 * @param device 장치 경로 (예: "/dev/ttyS0")
 * @return 성공 시 fd, 실패 시 -1
 */
int init_comm_uart(const std::string& device);

/* ─────────────────────────────────────────
   UART 공용 함수
───────────────────────────────────────── */

/**
 * @brief UART에서 패킷 한 개를 수신합니다. (STX~ETX + CRC 검증)
 * @param fd         UART 파일 디스크립터
 * @param out        수신된 프레임 [STX][CMD][LEN][DATA...][CRC][ETX]
 * @param timeout_ms 타임아웃 (ms)
 * @return CRC 검증 통과 시 true
 */
bool read_packet(int fd, std::vector<uint8_t>& out, int timeout_ms = 2000);

/* ─────────────────────────────────────────
   MQTT 공용 함수
───────────────────────────────────────── */

/**
 * @brief 임의 토픽으로 MQTT 메시지를 발행합니다.
 * @param topic   발행할 토픽
 * @param payload JSON 문자열
 */
void publish_to_server(const std::string& topic, const std::string& payload);

/* ─────────────────────────────────────────
   펌웨어 → 서버 (MQTT publish)
───────────────────────────────────────── */

/**
 * @brief CO/CO2 센서값을 MQTT로 서버에 전송합니다.
 *        토픽: config.json sensor_topic (기본: sensor/air_quality)
 */
void send_to_server_sensor(float co, float co2);

/* ─────────────────────────────────────────
   펌웨어 → STM32 (UART send)
───────────────────────────────────────── */

/**
 * @brief 온습도 데이터를 STM32에 전송합니다. (CMD 0x03)
 *        패킷: [AA][03][04][TEMP_H][TEMP_L][HUMI_H][HUMI_L][CRC][55]
 */
void send_to_stm32_temp_humi(int uart_fd, float temp, float humi);

/**
 * @brief WAV 파일 재생 명령을 STM32에 전송합니다. (CMD 0x20)
 *        패킷: [AA][20][LEN][filename...][CRC][55]
 * @param filename 재생할 WAV 파일명 (예: "alarm.wav")
 */
void send_to_stm32_play_wav(int uart_fd, const std::string& filename);

/**
 * @brief WAV 파일 목록을 STM32에 요청합니다. (CMD 0x21)
 * @return 파일명 목록, 실패 시 빈 벡터
 */
std::vector<std::string> send_to_stm32_get_wavs(int uart_fd);

/**
 * @brief 전체 구역 혼잡도를 STM32에 한 번에 전송합니다. (CMD 0x10)
 *        패킷: [AA][10][LEN][level0]...[levelN][CRC][55]
 * @param levels 각 구역 혼잡도 레벨 목록 (0: 여유, 1: 보통, 2: 혼잡, 최대 8개)
 */
void send_to_stm32_bulk_congestion(int uart_fd, const std::vector<int>& levels);

#endif // COMMUNICATION_HPP