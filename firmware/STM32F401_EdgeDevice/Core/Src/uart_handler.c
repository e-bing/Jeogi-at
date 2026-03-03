#include "uart_handler.h"
#include "uart_protocol.h"
#include "usart.h"
#include "services/audio_player.h"
#include "services/sd_storage.h"
#include "usart.h"
#include "services/audio_player.h"
#include "Matrixrun.h"

#include <stdio.h>
#include <string.h>

/* ─────────────────────────────────────────
내부 변수 / 상태
───────────────────────────────────────── */

#define RX_PARSER_TIMEOUT_MS 200U
#define UART_ISR_RAW_LOG 0

// 수신 상태머신 상태 정의
typedef enum
{
    STATE_WAIT_STX,  // STX 대기
    STATE_RECV_CMD,  // CMD 수신
    STATE_RECV_LEN,  // LEN 수신
    STATE_RECV_DATA, // DATA 수신
    STATE_RECV_CRC,  // CRC 수신
    STATE_WAIT_ETX   // ETX 대기 및 패킷 검증
} RxState_t;

static UART_HandleTypeDef *uart;
static uint8_t rx_buf[256];
static RxState_t rxState = STATE_WAIT_STX; // 현재 상태머신 상태
static Packet_t rxPkt;                     // 수신 중인 패킷
static uint8_t dataIdx;                    // DATA 필드 인덱스
static volatile uint8_t pktReady = 0;      // 패킷 수신 완료 플래그
static Packet_t pendingPkt;                // 처리 대기 중인 패킷
static uint32_t last_byte_tick = 0;

/* 수신 재arm 공통 함수
   - DMA 우선, 실패 시 IT fallback
   - HT 인터럽트 비활성화 유지 */
static void UART_RearmReception(void)
{
    HAL_StatusTypeDef st = HAL_UARTEx_ReceiveToIdle_DMA(uart, rx_buf, sizeof(rx_buf));
    if (st == HAL_OK)
    {
        if (uart->hdmarx != NULL)
        {
            __HAL_DMA_DISABLE_IT(uart->hdmarx, DMA_IT_HT);
        }
    }
    else
    {
        st = HAL_UARTEx_ReceiveToIdle_IT(uart, rx_buf, sizeof(rx_buf));
        printf("[UART] rearm fallback to IT, status=%d\r\n", st);
    }
}

/* ─────────────────────────────────────────
   UART 프로토콜 수신 초기화
   1) ReceiveToIdle + DMA 시도
   2) 실패 시 ReceiveToIdle + IT fallback
   3) HT(Half Transfer) 인터럽트 비활성화
───────────────────────────────────────── */
void UART_CMD_Init(UART_HandleTypeDef *huart)
{
    uart = huart;
    rxState = STATE_WAIT_STX;
    dataIdx = 0;
    pktReady = 0;
    last_byte_tick = HAL_GetTick();
    UART_RearmReception();
    printf("__uart__ init done\r\n");
}

/* ─────────────────────────────────────────
   Rx Event Callback
   - Idle 또는 지정 길이 도달 시 호출
   - 수신 바이트를 상태머신으로 전달
   - 콜백 끝에서만 DMA 재arm
───────────────────────────────────────── */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if ((uart == NULL) || (huart->Instance != uart->Instance))
    {
        return;
    }

    // 1) 상태머신 강제 리셋 (이전 패킷 찌꺼기 제거)
    rxState = STATE_WAIT_STX;
    dataIdx = 0;

#if UART_ISR_RAW_LOG
    printf("[UART6 RX] size=%u data=", Size);
    for (uint16_t i = 0; i < Size; i++)
    {
        printf("%02X ", rx_buf[i]);
    }
    printf("\r\n");
#endif

    for (uint16_t i = 0; i < Size; i++)
    {
        UART_RxCallback(rx_buf[i]);
    }

    // 3) 수신 중단 후 DMA 재시작 (Normal 모드 카운터 꼬임 방지)
    HAL_UART_AbortReceive(huart);
    memset(rx_buf, 0, sizeof(rx_buf));

    if (HAL_UARTEx_ReceiveToIdle_DMA(uart, rx_buf, sizeof(rx_buf)) == HAL_OK)
    {
        if (uart->hdmarx != NULL)
        {
            __HAL_DMA_DISABLE_IT(uart->hdmarx, DMA_IT_HT);
        }
    }
    else
    {
        HAL_UARTEx_ReceiveToIdle_IT(uart, rx_buf, sizeof(rx_buf));
    }

    printf("--- Waiting for Next Packet (10s) ---\r\n");
}

/* ─────────────────────────────────────────
   1바이트 상태머신 파서
   - STX/CMD/LEN/DATA/CRC/ETX 순서 파싱
   - CRC/ETX 통과 시 pktReady 설정
───────────────────────────────────────── */
void UART_RxCallback(uint8_t byte)
{
    switch (rxState)
    {
    case STATE_WAIT_STX:
        if (byte == PKT_STX)
            rxState = STATE_RECV_CMD;
        break;

    case STATE_RECV_CMD:
        rxPkt.cmd = byte;
        rxState = STATE_RECV_LEN;
        break;

    case STATE_RECV_LEN:
        rxPkt.len = byte;
        dataIdx = 0;

        if (rxPkt.len > PKT_MAX_DATA_LEN)
        {
            // 길이 오류: 즉시 리셋
            printf("LEN FAIL len=%u max=%u\r\n", rxPkt.len, PKT_MAX_DATA_LEN);
            rxState = STATE_WAIT_STX;
            break;
        }

        rxState = (rxPkt.len > 0) ? STATE_RECV_DATA : STATE_RECV_CRC;
        break;

    case STATE_RECV_DATA:
        rxPkt.data[dataIdx++] = byte; // len 검증을 앞에서 했으니 바로 저장
        if (dataIdx >= rxPkt.len)
            rxState = STATE_RECV_CRC;
        break;

    case STATE_RECV_CRC:
        rxPkt.crc = byte;
        rxState = STATE_WAIT_ETX;
        break;

    case STATE_WAIT_ETX:
        if (byte == PKT_ETX)
        {
            uint8_t c = Calc_CRC(&rxPkt);
            if (c == rxPkt.crc)
            {
                pktReady = 1;
                pendingPkt = rxPkt;
                printf("OK cmd=%02X len=%d\r\n", rxPkt.cmd, rxPkt.len);
            }
            else
            {
                printf("CRC FAIL calc=%02X rx=%02X cmd=%02X len=%d\r\n",
                       c, rxPkt.crc, rxPkt.cmd, rxPkt.len);
            }
        }
        else
        {
            printf("ETX FAIL got=%02X\r\n", byte);
        }
        rxState = STATE_WAIT_STX;
        break;
    }
}

/* UART 에러 복구 콜백
   - ORE/FE/NE/PE 등 발생 시 수신 경로를 재시작 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if ((uart == NULL) || (huart->Instance != uart->Instance))
    {
        return;
    }

    printf("[UART] error=0x%08lX, recover\r\n", huart->ErrorCode);
    rxState = STATE_WAIT_STX;
    dataIdx = 0;
    HAL_UART_DMAStop(huart);
    UART_RearmReception();
}

/* ─────────────────────────────────────────
   송신 패킷 조립/전송
   포맷: STX | CMD | LEN | DATA... | CRC | ETX
───────────────────────────────────────── */
static void SendPacket(uint8_t cmd, const uint8_t *data, uint8_t len)
{
    uint8_t buf[PKT_MAX_DATA_LEN + 6];
    uint8_t idx = 0;

    buf[idx++] = PKT_STX;
    buf[idx++] = cmd;
    buf[idx++] = len;
    for (int i = 0; i < len; i++)
        buf[idx++] = data[i];

    // CRC: STX 제외 나머지 XOR => cmd ^ len ^ data... ^ ETX
    uint8_t crc = cmd ^ len ^ PKT_ETX;
    for (int i = 0; i < len; i++)
        crc ^= data[i];

    buf[idx++] = crc;
    buf[idx++] = PKT_ETX;

    HAL_UART_Transmit(uart, buf, idx, 100);
}

/* ACK 패킷 전송 */
void UART_SendACK(uint8_t cmd)
{
    SendPacket(CMD_ACK, &cmd, 1);
}

/* NACK 패킷 전송 */
void UART_SendNACK(uint8_t cmd, uint8_t err)
{
    uint8_t data[2] = {cmd, err};
    SendPacket(CMD_NACK, data, 2);
}

/* 센서 응답 패킷 전송 */
void UART_SendSensorResp(uint8_t cmd, uint8_t *data, uint8_t len)
{
    uint8_t buf[PKT_MAX_DATA_LEN];
    buf[0] = cmd;
    for (uint8_t i = 0; i < len; i++)
    {
        buf[i + 1] = data[i];
    }
    SendPacket(CMD_RESP_SENSOR, buf, (uint8_t)(len + 1U));
}

/* ─────────────────────────────────────────
   메인 루프 처리 함수
   - pktReady 확인
   - cmd별 동작 분기
───────────────────────────────────────── */
void UART_Handler_Process(void)
{
    if (!pktReady)
    {
        return;
    }
    pktReady = 0;

    const Packet_t pkt = pendingPkt;

    switch (pkt.cmd)
    {
    case CMD_GET_CO:
        // 일산화탄소 센서 읽기 추가
        //        uint16_t ppm = Device_ReadCO();
        //        uint8_t resp[2] = {ppm >> 8, ppm & 0xFF};
        //        UART_SendSensorResp(CMD_GET_CO, resp, 2);
        break;

    case CMD_GET_CO2:
        // 이산화탄소 센서 읽기 추가
        //        uint16_t ppm = Device_ReadCO2();
        //        uint8_t resp[2] = {ppm >> 8, ppm & 0xFF};
        //        UART_SendSensorResp(CMD_GET_CO2, resp, 2);
        break;

    case CMD_GET_TEMP_HUM:
        // 온습도 센서 읽기 추가
        //        uint16_t temp, hum;
        //        Device_ReadTempHum(&temp, &hum);
        //        uint8_t resp[4] = {temp >> 8, temp & 0xFF, hum >> 8, hum & 0xFF};
        //        UART_SendSensorResp(CMD_GET_TEMP_HUM, resp, 4);
        break;

    case CMD_SET_LED:
        // LED 입력 추가 (벌크 혼잡도 업데이트)
        if (pkt.len != 8U)
        {
            UART_SendNACK(pkt.cmd, ERR_INVALID_DATA);
            break;
        }

        // 허용 레벨: 0(GREEN), 1(YELLOW), 2(RED)
        for (uint8_t i = 0; i < 8; i++)
        {
            if (pkt.data[i] > 2U)
            {
                UART_SendNACK(pkt.cmd, ERR_INVALID_DATA);
                return;
            }
        }

        // Matrix 상태 반영 후 ACK
        MatrixRun_SetCongestionBulk(pkt.data);
        printf("[UART] Bulk congestion update: P1=%d, P8=%d\r\n", pkt.data[0], pkt.data[7]);
        UART_SendACK(pkt.cmd);
        break;

    case CMD_PLAY_WAV:
        printf("[RECV] PLAY_WAV\r\n");
        if (pkt.len == 0 || pkt.len > 255)
        {
            UART_SendNACK(CMD_NACK, 1);
            break;
        }

        char filename[256];
        memcpy(filename, pkt.data, pkt.len);
        filename[pkt.len] = '\0';
        //        if (!wav_exists(filename)) { send_nack(ERR_NOT_FOUND); break; }
        Audio_PlayWav(filename);
        UART_SendNACK(CMD_ACK, 0);
        break;
    case CMD_GET_WAVS:
        printf("[RECV] GET_WAVS\r\n");

        uint8_t data[255];
        uint8_t len;

        sd_print_files();
        sd_read_files("/", data, &len);
        //			if (sd_read_files("/", data, &len) != FR_OK) return;
        SendPacket(CMD_RESP_WAVS, data, len);

        break;
    default:
        UART_SendNACK(pkt.cmd, ERR_INVALID_CMD);
        break;
    }
}
