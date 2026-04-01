#ifndef AUDIO_HPP
#define AUDIO_HPP

#include <string>
#include <vector>

/**
 * @brief STM32로부터 WAV 파일 목록을 가져와 내부에 저장합니다.
 *        프로그램 시작 시 한 번 호출하세요.
 * @param uart_fd STM32로의 UART 파일 디스크립터
 */
void init_audio(int uart_fd);

/**
 * @brief 파일명으로 WAV를 재생합니다.
 * @param filename 재생할 WAV 파일명 (예: "alarm.wav")
 */
void play_wav(const std::string& filename);

/**
 * @brief 인덱스로 WAV를 재생합니다. (init_audio에서 가져온 목록 기준)
 * @param index WAV 목록 인덱스 (0부터 시작)
 */
void play_wav(int index);

/**
 * @brief 저장된 WAV 파일 목록을 반환합니다.
 */
const std::vector<std::string>& get_wav_list();

/**
 * @brief 오디오 MQTT 구독을 초기화합니다.
 *        audio/control 토픽을 구독하여 서버 명령을 STM32로 전달합니다.
 */
void init_audio_mqtt();

#endif // AUDIO_HPP
