#include "services/audio_player.h"
#include "i2s.h"

#include <string.h>
#include <stdio.h>

/* ================= Queue Config ================= */

#define AUDIO_PLAYER_QUEUE_MAX   16U
#define AUDIO_PLAYER_NAME_MAX    32U

/* ================= Audio Buffers ================= */

/* mono PCM read buffer */
static int16_t s_monoBuf[AUDIO_PLAYER_MONO_SAMPLES];

/* stereo double buffer
 * half0: s_i2sBuf[0 ... AUDIO_PLAYER_MONO_SAMPLES*2 - 1]
 * half1: s_i2sBuf[AUDIO_PLAYER_MONO_SAMPLES*2 ... end]
 */
static int16_t s_i2sBuf[AUDIO_PLAYER_MONO_SAMPLES * 2 * 2];

/* ================= File ================= */

static FIL s_wavFile;

/* ================= Audio Queue ================= */

static char s_audioQueue[AUDIO_PLAYER_QUEUE_MAX][AUDIO_PLAYER_NAME_MAX];
static volatile uint8_t s_queueHead = 0;
static volatile uint8_t s_queueTail = 0;

/* ================= Playback State ================= */

static volatile uint8_t s_bufferEvent = 0;
static volatile uint8_t s_wavEof = 0;
static volatile uint8_t s_isPlaying = 0;
static volatile uint8_t s_stopPending = 0;

/* ================= Internal Function Prototypes ================= */

static void AudioPlayer_FillHalfBuffer(int16_t *dst);
static uint32_t AudioSource_ReadMonoPcm(int16_t *dst, uint32_t bytesToRead);

static uint8_t AudioPlayer_IsQueueEmpty(void);
static uint8_t AudioPlayer_IsQueueFull(void);
static void AudioPlayer_ClearQueue(void);
static uint8_t AudioPlayer_Enqueue(const char *filename);
static uint8_t AudioPlayer_Dequeue(char *out);
static void AudioPlayer_PlayNextFromQueue(void);

/* ================= Init ================= */

void Audio_Init(void)
{
    AudioPlayer_ClearQueue();

    s_bufferEvent = 0;
    s_wavEof = 0;
    s_isPlaying = 0;
    s_stopPending = 0;

    memset(s_monoBuf, 0, sizeof(s_monoBuf));
    memset(s_i2sBuf, 0, sizeof(s_i2sBuf));
}

/* ================= Queue ================= */

static uint8_t AudioPlayer_IsQueueEmpty(void)
{
    return (s_queueHead == s_queueTail);
}

static uint8_t AudioPlayer_IsQueueFull(void)
{
    return (((s_queueTail + 1U) % AUDIO_PLAYER_QUEUE_MAX) == s_queueHead);
}

static void AudioPlayer_ClearQueue(void)
{
    s_queueHead = 0;
    s_queueTail = 0;
}

static uint8_t AudioPlayer_Enqueue(const char *filename)
{
    if (AudioPlayer_IsQueueFull())
    {
        printf("Audio Queue Full\r\n");
        return 0;
    }

    strncpy(s_audioQueue[s_queueTail], filename, AUDIO_PLAYER_NAME_MAX - 1U);
    s_audioQueue[s_queueTail][AUDIO_PLAYER_NAME_MAX - 1U] = '\0';

    s_queueTail = (s_queueTail + 1U) % AUDIO_PLAYER_QUEUE_MAX;
    return 1;
}

static uint8_t AudioPlayer_Dequeue(char *out)
{
    if (AudioPlayer_IsQueueEmpty())
    {
        return 0;
    }

    strncpy(out, s_audioQueue[s_queueHead], AUDIO_PLAYER_NAME_MAX);
    out[AUDIO_PLAYER_NAME_MAX - 1U] = '\0';

    s_queueHead = (s_queueHead + 1U) % AUDIO_PLAYER_QUEUE_MAX;
    return 1;
}

static void AudioPlayer_PlayNextFromQueue(void)
{
    char filename[AUDIO_PLAYER_NAME_MAX];

    if (s_isPlaying)
    {
        return;
    }

    if (AudioPlayer_Dequeue(filename))
    {
        Audio_StartWav(filename);
    }
}

/* ================= Source Read ================= */

static uint32_t AudioSource_ReadMonoPcm(int16_t *dst, uint32_t bytesToRead)
{
    UINT bytesRead = 0;

    f_read(&s_wavFile, dst, bytesToRead, &bytesRead);
    return (uint32_t)bytesRead;
}

/* ================= Buffer Fill ================= */

static void AudioPlayer_FillHalfBuffer(int16_t *dst)
{
    uint32_t bytesRead;

    if (s_wavEof)
    {
        memset(dst, 0, AUDIO_PLAYER_MONO_SAMPLES * 2 * sizeof(int16_t));
        return;
    }

    bytesRead = AudioSource_ReadMonoPcm(
        s_monoBuf,
        AUDIO_PLAYER_MONO_SAMPLES * 2U
    );

    {
        int sampleCount = (int)(bytesRead / 2U);

        if (sampleCount == 0)
        {
            s_wavEof = 1;
            memset(dst, 0, AUDIO_PLAYER_MONO_SAMPLES * 2 * sizeof(int16_t));
            return;
        }

        /* mono -> stereo */
        for (int i = 0; i < sampleCount; i++)
        {
            int16_t sample = s_monoBuf[i];
            dst[i * 2]     = sample;
            dst[i * 2 + 1] = sample;
        }

        /* zero padding */
        for (int i = sampleCount; i < AUDIO_PLAYER_MONO_SAMPLES; i++)
        {
            dst[i * 2]     = 0;
            dst[i * 2 + 1] = 0;
        }

        if (sampleCount < AUDIO_PLAYER_MONO_SAMPLES)
        {
            s_wavEof = 1;
        }
    }
}

/* ================= DMA Callbacks ================= */

void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s == &hi2s3)
    {
        s_bufferEvent |= 0x01;
    }
}

void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s == &hi2s3)
    {
        s_bufferEvent |= 0x02;
    }
}

void HAL_I2S_ErrorCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s == &hi2s3)
    {
        printf("I2S Error\r\n");
    }
}

/* ================= Public API ================= */

uint8_t Audio_StartWav(const char *filename)
{
    if (s_isPlaying)
    {
        return 0;
    }

    s_bufferEvent = 0;
    s_wavEof = 0;
    s_stopPending = 0;

    if (f_open(&s_wavFile, filename, FA_READ) != FR_OK)
    {
        printf("WAV Open Fail: %s\r\n", filename);
        return 0;
    }

    printf("Play Start: %s\r\n", filename);

    if (f_lseek(&s_wavFile, AUDIO_PLAYER_WAV_HEADER_SIZE) != FR_OK)
    {
        f_close(&s_wavFile);
        return 0;
    }

    memset(s_monoBuf, 0, sizeof(s_monoBuf));
    memset(s_i2sBuf, 0, sizeof(s_i2sBuf));

    /* pre-fill */
    AudioPlayer_FillHalfBuffer(&s_i2sBuf[0]);
    AudioPlayer_FillHalfBuffer(&s_i2sBuf[AUDIO_PLAYER_MONO_SAMPLES * 2]);

    if (HAL_I2S_Transmit_DMA(&hi2s3,
                             (uint16_t *)s_i2sBuf,
                             AUDIO_PLAYER_MONO_SAMPLES * 2 * 2) != HAL_OK)
    {
        f_close(&s_wavFile);
        printf("I2S DMA Start Fail\r\n");
        return 0;
    }

    s_isPlaying = 1;
    return 1;
}

/* usage: Audio_Guide((uint8_t[8]){0, 1, 0, 1, 1, 0, 0, 0}); */
void Audio_Guide(const uint8_t *congestion)
{
    char has_zero = 0;
    char all_two = 1;

    for (int i = 0; i < 8; i++)
    {
        if (congestion[i] == 0)
        {
            if (!has_zero)
            {
                AudioPlayer_Enqueue("guide_start.wav");
                has_zero = 1;
            }

            char filename[AUDIO_PLAYER_NAME_MAX];
            snprintf(filename, sizeof(filename), "guide_%d.wav", i + 1);
            AudioPlayer_Enqueue(filename);
        }

        if (congestion[i] != 2)
        {
            all_two = 0;
        }
    }

    if (has_zero)
    {
        AudioPlayer_Enqueue("guide_end.wav");
    }
    else if (all_two)
    {
        AudioPlayer_Enqueue("warning.wav");
    }

    AudioPlayer_PlayNextFromQueue();
}

void Audio_Process(void)
{
    if (s_isPlaying)
    {
        if (s_bufferEvent & 0x01)
        {
            s_bufferEvent &= (uint8_t)~0x01;
            AudioPlayer_FillHalfBuffer(&s_i2sBuf[0]);

            if (s_wavEof)
            {
                s_stopPending = 1;
            }
        }

        if (s_bufferEvent & 0x02)
        {
            s_bufferEvent &= (uint8_t)~0x02;
            AudioPlayer_FillHalfBuffer(&s_i2sBuf[AUDIO_PLAYER_MONO_SAMPLES * 2]);

            if (s_wavEof)
            {
                s_stopPending = 1;
            }
        }

        if (s_stopPending && s_wavEof)
        {
            if (s_bufferEvent == 0)
            {
                Audio_Stop();
            }
        }
    }

    if (!s_isPlaying)
    {
        AudioPlayer_PlayNextFromQueue();
    }
}

void Audio_Stop(void)
{
    if (!s_isPlaying)
    {
        return;
    }

    HAL_I2S_DMAStop(&hi2s3);
    f_close(&s_wavFile);

    s_isPlaying   = 0;
    s_wavEof      = 0;
    s_bufferEvent = 0;
    s_stopPending = 0;

    printf("Play End\r\n");
}

uint8_t Audio_IsPlaying(void)
{
    return s_isPlaying;
}
