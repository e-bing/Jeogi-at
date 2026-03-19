/*
 * audio_player.h
 *
 *  Created on: 2026. 2. 27.
 *      Author: Taewoo Yun
 */

#ifndef INC_SERVICES_AUDIO_PLAYER_H_
#define INC_SERVICES_AUDIO_PLAYER_H_

#include "main.h"
#include "fatfs.h"
#include <stdint.h>

/* ================= Config ================= */

#define AUDIO_PLAYER_MONO_SAMPLES     4096U
#define AUDIO_PLAYER_WAV_HEADER_SIZE  44U

/* ================= Public API ================= */

void Audio_Init(void);
uint8_t Audio_StartWav(const char *filename);
void Audio_Process(void);
void Audio_Stop(void);
uint8_t Audio_IsPlaying(void);

void Audio_Guide(const uint8_t *congestion);

/* ================= Debug Counters ================= */

extern volatile uint32_t audio_i2s_err_cnt;
extern volatile uint32_t audio_miss_fill_cnt;

/* ================= WAV Header ================= */

typedef struct
{
    char riff[4];
    uint32_t size;
    char wave[4];

    char fmt[4];
    uint32_t fmt_size;
    uint16_t format;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t align;
    uint16_t bits;

    char data[4];
    uint32_t data_size;
} WAV_Header;


#endif /* INC_SERVICES_AUDIO_PLAYER_H_ */
