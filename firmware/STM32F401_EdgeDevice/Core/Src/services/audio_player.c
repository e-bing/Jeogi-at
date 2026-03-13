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

/* ================= Queue ================= */

#define AUDIO_QUEUE_MAX 16
#define AUDIO_NAME_MAX  32

static char audio_queue[AUDIO_QUEUE_MAX][AUDIO_NAME_MAX];
static volatile uint8_t q_head = 0;
static volatile uint8_t q_tail = 0;

/* ================= Status ================= */

volatile uint8_t buf_evt = 0;
volatile uint8_t wav_eof = 0;

volatile uint32_t audio_i2s_err_cnt = 0;
volatile uint32_t audio_miss_fill_cnt = 0;

static volatile uint8_t filling = 0;
static volatile uint8_t audio_playing = 0;

/* EOF 이후 마지막 DMA half 전송 완료까지 기다리기 위한 플래그 */
static volatile uint8_t stop_pending = 0;

/* ================= Internal ================= */

static void Audio_FillHalf(int16_t *dst);
static uint8_t Audio_QueueIsEmpty(void);
static uint8_t Audio_QueueIsFull(void);
static void Audio_QueueClear(void);
static uint8_t Audio_Enqueue(const char *filename);
static uint8_t Audio_Dequeue(char *out);
static void Audio_PlayNextFromQueue(void);

/* ================= Init ================= */

void Audio_Init(void)
{
    uint8_t buf[512];

    if (sd_init() == 0)
    {
        printf("SD init OK\r\n");

        if (sd_read_block(0, buf) == 0)
        {
            printf("Read OK\r\n");

            hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
            HAL_SPI_Init(&hspi2);
        }

        FRESULT res = f_mount(&USERFatFS, USERPath, 1);
        printf("FATFS mount = %d\r\n", res);
    }
}

/* ================= Queue ================= */

static uint8_t Audio_QueueIsEmpty(void)
{
    return (q_head == q_tail);
}

static uint8_t Audio_QueueIsFull(void)
{
    return (((q_tail + 1) % AUDIO_QUEUE_MAX) == q_head);
}

static void Audio_QueueClear(void)
{
    q_head = 0;
    q_tail = 0;
}

static uint8_t Audio_Enqueue(const char *filename)
{
    if (Audio_QueueIsFull())
    {
        printf("Audio Queue Full\r\n");
        return 0;
    }

    strncpy(audio_queue[q_tail], filename, AUDIO_NAME_MAX - 1);
    audio_queue[q_tail][AUDIO_NAME_MAX - 1] = '\0';
    q_tail = (q_tail + 1) % AUDIO_QUEUE_MAX;
    return 1;
}

static uint8_t Audio_Dequeue(char *out)
{
    if (Audio_QueueIsEmpty())
    {
        return 0;
    }

    strncpy(out, audio_queue[q_head], AUDIO_NAME_MAX);
    out[AUDIO_NAME_MAX - 1] = '\0';
    q_head = (q_head + 1) % AUDIO_QUEUE_MAX;
    return 1;
}

static void Audio_PlayNextFromQueue(void)
{
    char name[AUDIO_NAME_MAX];

    if (audio_playing)
    {
        return;
    }

    if (Audio_Dequeue(name))
    {
        Audio_StartWav(name);
    }
}

/* ================= Internal Fill ================= */

static void Audio_FillHalf(int16_t *dst)
{
    UINT br = 0;

    if (wav_eof)
    {
        memset(dst, 0, AUDIO_MONO_SAMPLES * 2 * sizeof(int16_t));
        return;
    }

    if (filling)
    {
        audio_miss_fill_cnt++;
    }

    filling = 1;

    /* Read mono PCM */
    f_read(&wav_file,
           mono_buf,
           AUDIO_MONO_SAMPLES * 2,
           &br);

    {
        int samples = br / 2;

        if (samples == 0)
        {
            wav_eof = 1;
            memset(dst, 0, AUDIO_MONO_SAMPLES * 2 * sizeof(int16_t));
            filling = 0;
            return;
        }

        /* mono -> stereo */
        for (int i = 0; i < samples; i++)
        {
            int16_t v = mono_buf[i];
            dst[i * 2] = v;
            dst[i * 2 + 1] = v;
        }

        /* 남는 구간 zero padding */
        for (int i = samples; i < AUDIO_MONO_SAMPLES; i++)
        {
            dst[i * 2] = 0;
            dst[i * 2 + 1] = 0;
        }

        if (samples < AUDIO_MONO_SAMPLES)
        {
            wav_eof = 1;
        }
    }

    filling = 0;
}

/* ================= DMA Callbacks ================= */

void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s == &hi2s3)
    {
        buf_evt |= 0x01;
    }
}

void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s == &hi2s3)
    {
        buf_evt |= 0x02;
    }
}

void HAL_I2S_ErrorCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s == &hi2s3)
    {
        audio_i2s_err_cnt++;
    }
}

/* ================= Public API ================= */

uint8_t Audio_StartWav(const char *filename)
{
    if (audio_playing)
    {
        /* 현재 재생 중이면 새로 시작하지 않음 */
        return 0;
    }

    buf_evt = 0;
    wav_eof = 0;
    filling = 0;
    stop_pending = 0;

    if (f_open(&wav_file, filename, FA_READ) != FR_OK)
    {
        printf("WAV Open Fail: %s\r\n", filename);
        return 0;
    }

    printf("Play Start: %s\r\n", filename);

    if (f_lseek(&wav_file, AUDIO_WAV_HEADER_SIZE) != FR_OK)
    {
        f_close(&wav_file);
        return 0;
    }

    memset(mono_buf, 0, sizeof(mono_buf));
    memset(i2s_buf, 0, sizeof(i2s_buf));

    /* pre-fill */
    Audio_FillHalf(&i2s_buf[0]);
    Audio_FillHalf(&i2s_buf[AUDIO_MONO_SAMPLES * 2]);

    if (HAL_I2S_Transmit_DMA(
            &hi2s3,
            (uint16_t *)i2s_buf,
            AUDIO_MONO_SAMPLES * 2 * 2) != HAL_OK)
    {
        f_close(&wav_file);
        printf("I2S DMA Start Fail\r\n");
        return 0;
    }

    audio_playing = 1;
    return 1;
}


// usage : Audio_Guide((char[8]){0, 1, 0, 1, 1, 0, 0, 0});
void Audio_Guide(const uint8_t *congestion)
{
    Audio_QueueClear();

    Audio_Enqueue("guide_start.wav");

    for (int i = 0; i < 8; i++)
    {
        if (congestion[i])
        {
            char name[AUDIO_NAME_MAX];
            sprintf(name, "guide_%d.wav", i + 1);
            Audio_Enqueue(name);
        }
    }

    Audio_Enqueue("guide_end.wav");

    Audio_PlayNextFromQueue();
}

void Audio_Process(void)
{
    if (audio_playing)
    {
        if (buf_evt & 0x01)
        {
            buf_evt &= ~0x01;
            Audio_FillHalf(&i2s_buf[0]);

            if (wav_eof)
            {
                stop_pending = 1;
            }
        }

        if (buf_evt & 0x02)
        {
            buf_evt &= ~0x02;
            Audio_FillHalf(&i2s_buf[AUDIO_MONO_SAMPLES * 2]);

            if (wav_eof)
            {
                stop_pending = 1;
            }
        }

        /* EOF가 발생한 뒤, 0 padding된 마지막 버퍼까지 흘려보낸 후 정지 */
        if (stop_pending && wav_eof)
        {
            /* 여기선 단순화해서 다음 complete 이벤트 이후 정지하도록 처리 */
            if (buf_evt == 0)
            {
                Audio_Stop();
            }
        }
    }

    if (!audio_playing)
    {
        Audio_PlayNextFromQueue();
    }
}

void Audio_Stop(void)
{
    if (!audio_playing)
    {
        return;
    }

    HAL_I2S_DMAStop(&hi2s3);
    f_close(&wav_file);

    audio_playing = 0;
    wav_eof = 0;
    buf_evt = 0;
    filling = 0;
    stop_pending = 0;

    printf("Play End\r\n");
}

uint8_t Audio_IsPlaying(void)
{
    return audio_playing;
}
