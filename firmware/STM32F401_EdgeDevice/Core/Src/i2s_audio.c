#include "spi_sdcard.h"
#include "i2s_audio.h"
#include "spi.h"
#include "i2s.h"

//#include <string.h>
//#include <stdio.h>
/* ================= Buffer ================= */

/* Mono input buffer */
static int16_t mono_buf[AUDIO_MONO_SAMPLES];

/* I2S DMA buffer (Double Buffer) */
/* [Half][L][R] */
static int16_t i2s_buf[AUDIO_MONO_SAMPLES * 2 * 2];

static FIL wav_file;

/* ================= Status ================= */

volatile uint8_t buf_evt = 0;
volatile uint8_t wav_eof = 0;

volatile uint32_t audio_i2s_err_cnt = 0;
volatile uint32_t audio_miss_fill_cnt = 0;

static volatile uint8_t filling = 0;

/* ================= Internal ================= */

static void Audio_FillHalf(int16_t *dst);

/* ================= Init ================= */

void Audio_Init(void)
{
    uint8_t buf[512];

    if (sd_init() == 0) {
        printf("SD init OK\r\n");

        if (sd_read_block(0, buf) == 0) {
            printf("Read OK\r\n");

            hspi2.Init.BaudRatePrescaler =
                SPI_BAUDRATEPRESCALER_4;
            HAL_SPI_Init(&hspi2);
        }
    }

    FRESULT res = f_mount(&USERFatFS, USERPath, 1);
    printf("FATFS mount = %d\r\n", res);

    sd_files();

    printf("Audio Init Done\r\n");
}

/* ================= Buffer Fill ================= */

static void Audio_FillHalf(int16_t *dst)
{
    UINT br;

    if (wav_eof) {
        memset(dst, 0, AUDIO_MONO_SAMPLES * 4);
        return;
    }

    /* Overlap detection */
    if (filling) {
        audio_miss_fill_cnt++;
    }

    filling = 1;

    uint32_t t0 = HAL_GetTick();

    /* Read mono PCM */
    f_read(&wav_file,
           mono_buf,
           AUDIO_MONO_SAMPLES * 2,
           &br);

    uint32_t dt = HAL_GetTick() - t0;

    printf("%dB read: %ld ms\r\n",
           AUDIO_MONO_SAMPLES,
           dt);

    int samples = br / 2;

    if (samples == 0) {
        wav_eof = 1;
        memset(dst, 0, AUDIO_MONO_SAMPLES * 4);
        filling = 0;
        return;
    }

    /* Mono → Stereo */
    for (int i = 0; i < samples; i++) {

        int16_t v = mono_buf[i];

        dst[i * 2]     = v;   // L
        dst[i * 2 + 1] = v;   // R
    }

    /* Padding */
    if (samples < AUDIO_MONO_SAMPLES) {

        for (int i = samples; i < AUDIO_MONO_SAMPLES; i++) {

            dst[i * 2]     = 0;
            dst[i * 2 + 1] = 0;
        }

        wav_eof = 1;
    }

    filling = 0;
}

/* ================= DMA Callbacks ================= */

void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s == &hi2s3) {
        buf_evt |= 0x01;
    }
}

void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s == &hi2s3) {
        buf_evt |= 0x02;
    }
}

void HAL_I2S_ErrorCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s == &hi2s3) {
        audio_i2s_err_cnt++;
    }
}

/* ================= Player ================= */

void Audio_PlayWav(const char *filename)
{
    buf_evt = 0;
    wav_eof = 0;

    if (f_open(&wav_file, filename, FA_READ) != FR_OK) {

        printf("WAV Open Fail\r\n");
        return;
    }

    /* Skip WAV header */
    f_lseek(&wav_file, AUDIO_WAV_HEADER_SIZE);

    /* Pre-fill buffers */
    Audio_FillHalf(&i2s_buf[0]);
    Audio_FillHalf(&i2s_buf[AUDIO_MONO_SAMPLES * 2]);

    /* Start DMA */
    HAL_I2S_Transmit_DMA(
        &hi2s3,
        (uint16_t *)i2s_buf,
        AUDIO_MONO_SAMPLES * 2 * 2
    );

    printf("Play Start\r\n");

    /* Playback loop */
    while (!wav_eof) {

        if (buf_evt & 0x01) {

            buf_evt &= ~0x01;
            Audio_FillHalf(&i2s_buf[0]);
        }

        if (buf_evt & 0x02) {

            buf_evt &= ~0x02;
            Audio_FillHalf(
                &i2s_buf[AUDIO_MONO_SAMPLES * 2]
            );
        }
    }

    HAL_Delay(10);

    HAL_I2S_DMAStop(&hi2s3);
    f_close(&wav_file);

    printf("Play End\r\n");
}
