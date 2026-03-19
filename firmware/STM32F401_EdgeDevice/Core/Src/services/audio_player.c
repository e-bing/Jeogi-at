#include "services/audio_player.h"
#include "services/audio_stream_rx.h"   /* SPI RX ring buffer 모듈 */
#include "i2s.h"

#include <string.h>
#include <stdio.h>

/* ================= Queue Config ================= */

#define AUDIO_PLAYER_QUEUE_MAX   16U
#define AUDIO_PLAYER_NAME_MAX    32U

/*
 * stream 재생 시작 전 최소 prebuffer 크기
 * 현재 SPI 수신 블록이 960 bytes(= 10ms @ 48k mono 16bit) 기준이므로
 * 4블록 = 40ms 정도 쌓인 뒤 시작하도록 설정
 */
#define AUDIO_STREAM_PREBUFFER_BYTES   (1920U * 4U)

/* ================= Audio Buffers ================= */

/* mono PCM read buffer
 * WAV 파일이든 SPI stream이든 먼저 mono PCM을 여기로 읽는다.
 */
static int16_t s_monoBuf[AUDIO_PLAYER_MONO_SAMPLES];

/* stereo double buffer
 * I2S로 내보내는 최종 버퍼
 *
 * half0: s_i2sBuf[0 ... AUDIO_PLAYER_MONO_SAMPLES*2 - 1]
 * half1: s_i2sBuf[AUDIO_PLAYER_MONO_SAMPLES*2 ... end]
 *
 * mono 샘플을 L/R로 복제해서 채운다.
 */
static int16_t s_i2sBuf[AUDIO_PLAYER_MONO_SAMPLES * 2 * 2];

/* ================= File ================= */

static FIL s_wavFile;

/* ================= Audio Queue ================= */

static char s_audioQueue[AUDIO_PLAYER_QUEUE_MAX][AUDIO_PLAYER_NAME_MAX];
static volatile uint8_t s_queueHead = 0;
static volatile uint8_t s_queueTail = 0;

/* ================= Playback Source ================= */

/*
 * [추가]
 * 현재 재생 source가 무엇인지 구분하기 위한 상태
 *
 * - WAV   : SD의 WAV 파일을 읽어 재생
 * - STREAM: SPI로 들어온 PCM stream을 재생
 */
typedef enum
{
    AUDIO_SOURCE_NONE = 0,
    AUDIO_SOURCE_WAV,
    AUDIO_SOURCE_STREAM
} AudioSource_t;

static volatile AudioSource_t s_audioSource = AUDIO_SOURCE_NONE;

/* ================= Playback State ================= */

static volatile uint8_t s_bufferEvent = 0;
static volatile uint8_t s_wavEof = 0;
static volatile uint8_t s_isPlaying = 0;
static volatile uint8_t s_stopPending = 0;

/*
 * [추가]
 * stream은 WAV처럼 EOF가 없으므로,
 * 외부에서 "이제 stream 재생을 정리하자"라고 요청할 때 쓰는 플래그
 *
 * 현재 1차 버전에서는 꼭 안 써도 되지만,
 * 나중에 방송 종료 시 안전하게 정지할 수 있도록 넣어둠.
 */
static volatile uint8_t s_streamStopRequested = 0;

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

    s_audioSource = AUDIO_SOURCE_NONE;
    s_bufferEvent = 0;
    s_wavEof = 0;
    s_isPlaying = 0;
    s_stopPending = 0;
    s_streamStopRequested = 0;

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

/*
 * [핵심 추가]
 * mono PCM을 읽는 공급원을 source별로 분기
 *
 * - WAV   : f_read()로 SD 파일에서 읽음
 * - STREAM: AudioStreamRx_Read()로 SPI ring buffer에서 읽음
 *
 * bytesToRead는 "바이트 단위"이다.
 */
static uint32_t AudioSource_ReadMonoPcm(int16_t *dst, uint32_t bytesToRead)
{
    if (s_audioSource == AUDIO_SOURCE_WAV)
    {
        UINT bytesRead = 0;
        f_read(&s_wavFile, dst, bytesToRead, &bytesRead);
        return (uint32_t)bytesRead;
    }
    else if (s_audioSource == AUDIO_SOURCE_STREAM)
    {
        return AudioStreamRx_Read((uint8_t *)dst, bytesToRead);
    }

    return 0;
}

/* ================= Buffer Fill ================= */

/*
 * I2S half-buffer 1개를 채우는 공통 함수
 *
 * 동작:
 * 1) source(WAV or STREAM)에서 mono PCM 읽음
 * 2) mono -> stereo 복제
 * 3) 부족한 부분은 zero padding
 *
 * 주의:
 * - WAV는 sample 부족을 EOF로 처리
 * - STREAM은 sample 부족을 "일시적 underrun"으로 보고 무음으로 채움
 */
static void AudioPlayer_FillHalfBuffer(int16_t *dst)
{
    uint32_t bytesRead;

    /* WAV 재생에서 이미 EOF가 확정된 경우는 무음만 채움 */
    if ((s_audioSource == AUDIO_SOURCE_WAV) && s_wavEof)
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

        /* 아무 샘플도 못 읽은 경우 */
        if (sampleCount == 0)
        {
            if (s_audioSource == AUDIO_SOURCE_WAV)
            {
                /* WAV는 EOF 처리 */
                s_wavEof = 1;
            }
            else
            {
                /*
                 * STREAM은 EOF 개념이 없으므로
                 * 일단 무음으로 채워서 재생 continuity를 유지
                 */
            }

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

        /*
         * WAV는 half-buffer를 꽉 못 채우면 EOF로 봄
         * STREAM은 그냥 순간 버퍼 부족으로 보고 넘어감
         */
        if ((s_audioSource == AUDIO_SOURCE_WAV) &&
            (sampleCount < AUDIO_PLAYER_MONO_SAMPLES))
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

    s_audioSource = AUDIO_SOURCE_WAV;
    s_bufferEvent = 0;
    s_wavEof = 0;
    s_stopPending = 0;
    s_streamStopRequested = 0;

    if (f_open(&s_wavFile, filename, FA_READ) != FR_OK)
    {
        s_audioSource = AUDIO_SOURCE_NONE;
        printf("WAV Open Fail: %s\r\n", filename);
        return 0;
    }

    printf("Play Start: %s\r\n", filename);

    if (f_lseek(&s_wavFile, AUDIO_PLAYER_WAV_HEADER_SIZE) != FR_OK)
    {
        f_close(&s_wavFile);
        s_audioSource = AUDIO_SOURCE_NONE;
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
        s_audioSource = AUDIO_SOURCE_NONE;
        printf("I2S DMA Start Fail\r\n");
        return 0;
    }

    s_isPlaying = 1;
    return 1;
}

/*
 * [추가]
 * SPI로 누적된 PCM stream 재생 시작
 *
 * 시작 조건:
 * - 현재 다른 재생 중이 아니어야 함
 * - ring buffer에 최소 prebuffer가 쌓여 있어야 함
 */
uint8_t Audio_StartStream(void)
{
    if (s_isPlaying)
    {
        return 0;
    }

    if (AudioStreamRx_Available() < AUDIO_STREAM_PREBUFFER_BYTES)
    {
        return 0;
    }

    s_audioSource = AUDIO_SOURCE_STREAM;
    s_bufferEvent = 0;
    s_wavEof = 0;
    s_stopPending = 0;
    s_streamStopRequested = 0;

    memset(s_monoBuf, 0, sizeof(s_monoBuf));
    memset(s_i2sBuf, 0, sizeof(s_i2sBuf));

    /* pre-fill */
    AudioPlayer_FillHalfBuffer(&s_i2sBuf[0]);
    AudioPlayer_FillHalfBuffer(&s_i2sBuf[AUDIO_PLAYER_MONO_SAMPLES * 2]);

    if (HAL_I2S_Transmit_DMA(&hi2s3,
                             (uint16_t *)s_i2sBuf,
                             AUDIO_PLAYER_MONO_SAMPLES * 2 * 2) != HAL_OK)
    {
        s_audioSource = AUDIO_SOURCE_NONE;
        printf("I2S DMA Start Fail (Stream)\r\n");
        return 0;
    }

    printf("Stream Play Start\r\n");
    s_isPlaying = 1;
    return 1;
}

/*
 * [추가]
 * stream 재생 종료 요청
 *
 * 바로 끊는 게 아니라,
 * Audio_Process()에서 안전한 시점에 정지하도록 플래그만 세움
 */
void Audio_RequestStopStream(void)
{
    if (s_audioSource == AUDIO_SOURCE_STREAM)
    {
        s_streamStopRequested = 1;
    }
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

    /* guide 재생 요청이 들어오면 WAV 우선 */
    AudioPlayer_PlayNextFromQueue();
}

void Audio_Process(void)
{
	AudioStreamRx_Process();

    if (s_isPlaying)
    {
        if (s_bufferEvent & 0x01)
        {
            s_bufferEvent &= (uint8_t)~0x01;
            AudioPlayer_FillHalfBuffer(&s_i2sBuf[0]);

            if ((s_audioSource == AUDIO_SOURCE_WAV) && s_wavEof)
            {
                s_stopPending = 1;
            }
        }

        if (s_bufferEvent & 0x02)
        {
            s_bufferEvent &= (uint8_t)~0x02;
            AudioPlayer_FillHalfBuffer(&s_i2sBuf[AUDIO_PLAYER_MONO_SAMPLES * 2]);

            if ((s_audioSource == AUDIO_SOURCE_WAV) && s_wavEof)
            {
                s_stopPending = 1;
            }
        }

        /*
         * WAV 재생은 EOF 기반으로 종료
         */
        if ((s_audioSource == AUDIO_SOURCE_WAV) && s_stopPending && s_wavEof)
        {
            if (s_bufferEvent == 0)
            {
                Audio_Stop();
            }
        }

        /*
         * STREAM 재생은 EOF가 없으므로
         * 외부에서 stop 요청이 들어왔을 때만 종료 처리
         */
        if ((s_audioSource == AUDIO_SOURCE_STREAM) && s_streamStopRequested)
        {
            if ((AudioStreamRx_Available() == 0U) && (s_bufferEvent == 0U))
            {
                Audio_Stop();
            }
        }
    }

    /*
     * 재생 중이 아니면 우선순위:
     * 1) guide queue 재생
     * 2) guide가 없으면 stream 재생 시도
     */
    if (!s_isPlaying)
    {
        if (!AudioPlayer_IsQueueEmpty())
        {
            AudioPlayer_PlayNextFromQueue();
        }
        else
        {
            /* 충분히 prebuffer가 쌓였을 때만 stream 시작 */
            Audio_StartStream();
        }
    }
}

void Audio_Stop(void)
{
    if (!s_isPlaying)
    {
        return;
    }

    HAL_I2S_DMAStop(&hi2s3);

    if (s_audioSource == AUDIO_SOURCE_WAV)
    {
        f_close(&s_wavFile);
    }

    s_isPlaying = 0;
    s_wavEof = 0;
    s_bufferEvent = 0;
    s_stopPending = 0;
    s_streamStopRequested = 0;
    s_audioSource = AUDIO_SOURCE_NONE;

    printf("Play End\r\n");
}

uint8_t Audio_IsPlaying(void)
{
    return s_isPlaying;
}
