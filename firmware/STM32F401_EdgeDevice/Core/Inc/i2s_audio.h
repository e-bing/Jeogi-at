#ifndef __I2S_AUDIO_H
#define __I2S_AUDIO_H

#include "main.h"
#include "fatfs.h"
#include <stdint.h>

/* ================= Config ================= */

#define AUDIO_MONO_SAMPLES     8192
#define AUDIO_WAV_HEADER_SIZE  44

/* ================= Public API ================= */

void Audio_Init(void);
void Audio_PlayWav(const char *filename);

/* ================= Debug ================= */

extern volatile uint32_t audio_i2s_err_cnt;
extern volatile uint32_t audio_miss_fill_cnt;

#endif /* __I2S_AUDIO_H */
