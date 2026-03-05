#include "audio.hpp"
#include "communication.hpp"
#include <iostream>

using namespace std;

static int              g_audio_uart_fd = -1;
static vector<string>   g_wav_list;

/**
 * @brief STM32로부터 WAV 파일 목록을 가져와 내부에 저장합니다.
 */
void init_audio(int uart_fd) {
    g_audio_uart_fd = uart_fd;

    g_wav_list = send_to_stm32_get_wavs(uart_fd);

    for (auto& f : g_wav_list)
        cout << " - " << f << "\n";
}

/**
 * @brief 파일명으로 WAV를 재생합니다.
 */
void play_wav(const string& filename) {
    if (g_audio_uart_fd < 0) {
        cerr << "[Audio] UART 미초기화 - play_wav 불가" << endl;
        return;
    }
    send_to_stm32_play_wav(g_audio_uart_fd, filename);
}

/**
 * @brief 인덱스로 WAV를 재생합니다.
 */
void play_wav(int index) {
    if (index < 0 || index >= static_cast<int>(g_wav_list.size())) {
        cerr << "[Audio] 잘못된 인덱스: " << index << endl;
        return;
    }
    play_wav(g_wav_list[index]);
}

/**
 * @brief 저장된 WAV 파일 목록을 반환합니다.
 */
const vector<string>& get_wav_list() {
    return g_wav_list;
}
