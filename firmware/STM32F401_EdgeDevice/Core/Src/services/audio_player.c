#include "services/audio_player.h"
#include "drivers/storage/sd_spi.h"
#include "spi.h"
#include "i2s.h"

#include <string.h>
#include <stdio.h>

/* ================= Queue Config ================= */

#define AUDIO_PLAYER_QUEUE_MAX   16U
#define AUDIO_PLAYER_NAME_MAX    32U

/* ================= Stream Buffer Config ================= */

#define AUDIO_STREAM_BUFFER_SIZE   (32768U)

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
static volatile uint8_t s_isFilling = 0;
static volatile uint8_t s_isPlaying = 0;
static volatile uint8_t s_stopPending = 0;

/* ================= Source Mode ================= */

static volatile AudioSource_t s_audioSource = AUDIO_SOURCE_SD;

/* ================= Stream Ring Buffer ================= */

static uint8_t s_streamBuf[AUDIO_STREAM_BUFFER_SIZE];
static volatile uint32_t s_streamWrite = 0;
static volatile uint32_t s_streamRead  = 0;
static volatile uint32_t s_streamCount = 0;

/* ================= Debug Counters ================= */

volatile uint32_t audio_i2s_err_cnt = 0;
volatile uint32_t audio_miss_fill_cnt = 0;
volatile uint32_t audio_stream_underrun_cnt = 0;
volatile uint32_t audio_stream_overflow_cnt = 0;

/* ================= Internal Function Prototypes ================= */

static void AudioPlayer_FillHalfBuffer(int16_t *dst);
static uint32_t AudioSource_ReadMonoPcm(int16_t *dst, uint32_t bytesToRead);

static uint8_t AudioPlayer_IsQueueEmpty(void);
static uint8_t AudioPlayer_IsQueueFull(void);
static void AudioPlayer_ClearQueue(void);
static uint8_t AudioPlayer_Enqueue(const char *filename);
static uint8_t AudioPlayer_Dequeue(char *out);
static void AudioPlayer_PlayNextFromQueue(void);

static void AudioStream_Reset(void);
static uint32_t AudioStream_GetCount(void);

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

        {
            FRESULT res = f_mount(&USERFatFS, USERPath, 1);
            printf("FATFS mount = %d\r\n", res);
        }
    }

    AudioStream_Reset();
}

/* ================= Source API ================= */

void Audio_SetSource(AudioSource_t source)
{
    s_audioSource = source;
}

AudioSource_t Audio_GetSource(void)
{
    return s_audioSource;
}

/* ================= Stream Buffer ================= */

static void AudioStream_Reset(void)
{
    s_streamWrite = 0;
    s_streamRead = 0;
    s_streamCount = 0;
}

static uint32_t AudioStream_GetCount(void)
{
    return s_streamCount;
}

/* SPI 수신 완료 시 호출해서 PCM byte를 밀어넣는 함수 */
uint32_t Audio_StreamWrite(const uint8_t *src, uint32_t len)
{
    uint32_t written = 0;

    while (written < len)
    {
        if (s_streamCount >= AUDIO_STREAM_BUFFER_SIZE)
        {
            audio_stream_overflow_cnt++;
            break;
        }

        s_streamBuf[s_streamWrite] = src[written];
        s_streamWrite = (s_streamWrite + 1U) % AUDIO_STREAM_BUFFER_SIZE;
        s_streamCount++;
        written++;
    }

    return written;
}

static uint32_t AudioStream_Read(uint8_t *dst, uint32_t len)
{
    uint32_t read = 0;

    while (read < len)
    {
        if (s_streamCount == 0U)
        {
            break;
        }

        dst[read] = s_streamBuf[s_streamRead];
        s_streamRead = (s_streamRead + 1U) % AUDIO_STREAM_BUFFER_SIZE;
        s_streamCount--;
        read++;
    }

    return read;
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
    if (s_audioSource == AUDIO_SOURCE_SD)
    {
        UINT bytesRead = 0;

        f_read(&s_wavFile, dst, bytesToRead, &bytesRead);
        return (uint32_t)bytesRead;
    }
    else
    {
        return AudioStream_Read((uint8_t *)dst, bytesToRead);
    }
}

/* ================= Buffer Fill ================= */

static void AudioPlayer_FillHalfBuffer(int16_t *dst)
{
    uint32_t bytesRead = 0;

    if (s_audioSource == AUDIO_SOURCE_SD)
    {
        if (s_wavEof)
        {
            memset(dst, 0, AUDIO_PLAYER_MONO_SAMPLES * 2 * sizeof(int16_t));
            return;
        }
    }

    if (s_isFilling)
    {
        audio_miss_fill_cnt++;
    }

    s_isFilling = 1;

    bytesRead = AudioSource_ReadMonoPcm(
        s_monoBuf,
        AUDIO_PLAYER_MONO_SAMPLES * 2U
    );

    {
        int sampleCount = (int)(bytesRead / 2U);

        if (sampleCount == 0)
        {
            /* SD 모드면 EOF, Stream 모드면 underrun */
            if (s_audioSource == AUDIO_SOURCE_SD)
            {
                s_wavEof = 1;
            }
            else
            {
                audio_stream_underrun_cnt++;
            }

            memset(dst, 0, AUDIO_PLAYER_MONO_SAMPLES * 2 * sizeof(int16_t));
            s_isFilling = 0;
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

        if (s_audioSource == AUDIO_SOURCE_SD)
        {
            if (sampleCount < AUDIO_PLAYER_MONO_SAMPLES)
            {
                s_wavEof = 1;
            }
        }
        else
        {
            if (sampleCount < AUDIO_PLAYER_MONO_SAMPLES)
            {
                audio_stream_underrun_cnt++;
            }
        }
    }

    s_isFilling = 0;
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
        audio_i2s_err_cnt++;
    }
}

/* ================= Public API ================= */

uint8_t Audio_StartWav(const char *filename)
{
    if (s_isPlaying)
    {
        return 0;
    }

    Audio_SetSource(AUDIO_SOURCE_SD);

    s_bufferEvent = 0;
    s_wavEof = 0;
    s_isFilling = 0;
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

uint8_t Audio_StartStream(void)
{
    uint32_t prebufferBytes;

    if (s_isPlaying)
    {
        return 0;
    }

    Audio_SetSource(AUDIO_SOURCE_STREAM);

    s_bufferEvent = 0;
    s_wavEof = 0;
    s_isFilling = 0;
    s_stopPending = 0;

    memset(s_monoBuf, 0, sizeof(s_monoBuf));
    memset(s_i2sBuf, 0, sizeof(s_i2sBuf));

    /* 2 half-buffer 이상 쌓였을 때 시작 권장 */
    prebufferBytes = AUDIO_PLAYER_MONO_SAMPLES * 2U * 2U;

    if (AudioStream_GetCount() < prebufferBytes)
    {
        return 0;
    }

    /* pre-fill */
    AudioPlayer_FillHalfBuffer(&s_i2sBuf[0]);
    AudioPlayer_FillHalfBuffer(&s_i2sBuf[AUDIO_PLAYER_MONO_SAMPLES * 2]);

    if (HAL_I2S_Transmit_DMA(&hi2s3,
                             (uint16_t *)s_i2sBuf,
                             AUDIO_PLAYER_MONO_SAMPLES * 2 * 2) != HAL_OK)
    {
        printf("I2S DMA Start Fail (Stream)\r\n");
        return 0;
    }

    printf("Stream Play Start\r\n");
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

            if ((s_audioSource == AUDIO_SOURCE_SD) && s_wavEof)
            {
                s_stopPending = 1;
            }
        }

        if (s_bufferEvent & 0x02)
        {
            s_bufferEvent &= (uint8_t)~0x02;
            AudioPlayer_FillHalfBuffer(&s_i2sBuf[AUDIO_PLAYER_MONO_SAMPLES * 2]);

            if ((s_audioSource == AUDIO_SOURCE_SD) && s_wavEof)
            {
                s_stopPending = 1;
            }
        }

        /* SD 파일 재생일 때만 EOF로 stop */
        if ((s_audioSource == AUDIO_SOURCE_SD) && s_stopPending && s_wavEof)
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

    // run: streaming
//    if (!s_isPlaying && s_audioSource == AUDIO_SOURCE_STREAM)
	if (s_audioSource == AUDIO_SOURCE_STREAM)
    {
        uint32_t prebufferBytes = AUDIO_PLAYER_MONO_SAMPLES * 2U * 2U;

        if (AudioStream_GetCount() >= prebufferBytes)
        {
            if (Audio_StartStream())
            {
                printf("Stream Play Triggered\r\n");
            }
        }
    }
    //
}

void Audio_Stop(void)
{
    if (!s_isPlaying)
    {
        return;
    }

    HAL_I2S_DMAStop(&hi2s3);

    if (s_audioSource == AUDIO_SOURCE_SD)
    {
        f_close(&s_wavFile);
    }

    s_isPlaying   = 0;
    s_wavEof      = 0;
    s_bufferEvent = 0;
    s_isFilling   = 0;
    s_stopPending = 0;

    printf("Play End\r\n");
}

uint8_t Audio_IsPlaying(void)
{
    return s_isPlaying;
}
