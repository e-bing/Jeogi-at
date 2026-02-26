#ifndef AUDIO_OUT_H
#define AUDIO_OUT_H

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

typedef struct {
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

#endif /* AUDIO_OUT_H */
