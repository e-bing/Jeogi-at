#include "services/audio_player.h"
#include "drivers/storage/sd_spi.h"
#include "spi.h"
#include "i2s.h"
#include <string.h>
#include <stdio.h>

/* ================= Buffer ================= */

static int16_t mono_buf[AUDIO_MONO_SAMPLES];

/* stereo double buffer
   half0: i2s_buf[0 ... AUDIO_MONO_SAMPLES*2 - 1]
   half1: i2s_buf[AUDIO_MONO_SAMPLES*2 ... end]
*/
static int16_t i2s_buf[AUDIO_MONO_SAMPLES * 2 * 2];

static FIL wav_file;

/* ================= Status ================= */

volatile uint8_t buf_evt = 0;
volatile uint8_t wav_eof = 0;

volatile uint32_t audio_i2s_err_cnt = 0;
volatile uint32_t audio_miss_fill_cnt = 0;

static volatile uint8_t filling = 0;
static volatile uint8_t audio_playing = 0;

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

            hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
            HAL_SPI_Init(&hspi2);
        }

        FRESULT res = f_mount(&USERFatFS, USERPath, 1);
        printf("FATFS mount = %d\r\n", res);
    }
}

/* ================= Internal Fill ================= */

static void Audio_FillHalf(int16_t *dst)
{
    UINT br = 0;

    if (wav_eof) {
        memset(dst, 0, AUDIO_MONO_SAMPLES * 2 * sizeof(int16_t));
        return;
    }

    if (filling) {
        audio_miss_fill_cnt++;
    }

    filling = 1;

    /* 재생 중에는 printf 빼는 게 안전 */
    if (f_read(&wav_file, mono_buf, AUDIO_MONO_SAMPLES * sizeof(int16_t), &br) != FR_OK) {
        br = 0;
    }

    int samples = br / sizeof(int16_t);

    if (samples == 0) {
        wav_eof = 1;
        memset(dst, 0, AUDIO_MONO_SAMPLES * 2 * sizeof(int16_t));
        filling = 0;
        return;
    }

    /* mono -> stereo */
    for (int i = 0; i < samples; i++) {
        int16_t v = mono_buf[i];
        dst[i * 2]     = v;
        dst[i * 2 + 1] = v;
    }

    /* 남는 구간 0 padding */
    for (int i = samples; i < AUDIO_MONO_SAMPLES; i++) {
        dst[i * 2]     = 0;
        dst[i * 2 + 1] = 0;
    }

    if (samples < AUDIO_MONO_SAMPLES) {
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

/* ================= Public API ================= */

uint8_t Audio_StartWav(const char *filename)
{
    if (audio_playing) {
        Audio_Stop();
    }

    buf_evt = 0;
    wav_eof = 0;
    filling = 0;
    audio_playing = 0;

    if (f_open(&wav_file, filename, FA_READ) != FR_OK) {
        printf("WAV Open Fail\r\n");
        return 0;
    }

    printf("Play Start: %s\r\n", filename);

    if (f_lseek(&wav_file, AUDIO_WAV_HEADER_SIZE) != FR_OK) {
        f_close(&wav_file);
        return 0;
    }

    /* pre-fill */
    Audio_FillHalf(&i2s_buf[0]);
    Audio_FillHalf(&i2s_buf[AUDIO_MONO_SAMPLES * 2]);

    if (HAL_I2S_Transmit_DMA(
            &hi2s3,
            (uint16_t *)i2s_buf,
            AUDIO_MONO_SAMPLES * 2 * 2) != HAL_OK) {

        f_close(&wav_file);
        printf("I2S DMA Start Fail\r\n");
        return 0;
    }

    audio_playing = 1;
    return 1;
}

void Audio_Process(void)
{
    if (!audio_playing) {
        return;
    }

    if (buf_evt & 0x01) {
        buf_evt &= ~0x01;
        Audio_FillHalf(&i2s_buf[0]);
    }

    if (buf_evt & 0x02) {
        buf_evt &= ~0x02;
        Audio_FillHalf(&i2s_buf[AUDIO_MONO_SAMPLES * 2]);
    }

    /* EOF 이후 두 half가 모두 소진될 시간을 고려해 stop을 조금 늦출 수도 있지만
       우선은 단순하게: EOF 상태이고 refill 이벤트까지 처리됐으면 종료 */
    if (wav_eof && buf_evt == 0) {
        Audio_Stop();
    }
}

void Audio_Stop(void)
{
    if (!audio_playing) {
        return;
    }

    HAL_I2S_DMAStop(&hi2s3);
    f_close(&wav_file);

    audio_playing = 0;
    wav_eof = 0;
    buf_evt = 0;
    filling = 0;

    printf("Play End\r\n");
}

uint8_t Audio_IsPlaying(void)
{
    return audio_playing;
}
