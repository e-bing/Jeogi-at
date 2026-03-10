#include "uart_handler.h"
#include "uart_protocol.h"
#include "Data_Manager.h"
#include "usart.h" // CubeMX 생성 파일 (huart2 등)
#include "services/audio_player.h"
#include "services/sd_storage.h"
// #include "led_panel.h"
#include "mq7.h"
#include "mq135.h"
// #include "sht20.h"

/* ─────────────────────────────────────────
   내부 변수
───────────────────────────────────────── */

static UART_HandleTypeDef *uart; // Init 시 등록된 UART 핸들
static uint8_t rx_buf[64];       // DMA 수신 버퍼

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

static RxState_t rxState = STATE_WAIT_STX; // 현재 상태머신 상태
static Packet_t rxPkt;                     // 수신 중인 패킷
static uint8_t dataIdx;                    // DATA 필드 인덱스
static volatile uint8_t pktReady = 0;      // 패킷 수신 완료 플래그
static Packet_t pendingPkt;                // 처리 대기 중인 패킷

/* ─────────────────────────────────────────
   초기화 - DMA 수신 시작
───────────────────────────────────────── */
void UART_CMD_Init(UART_HandleTypeDef *huart)
{
    uart = huart; // 정해놓은 uart 사용
    HAL_UARTEx_ReceiveToIdle_DMA(uart, rx_buf, sizeof(rx_buf));
    printf("__uart__ __init__\r\n");
}

/* ─────────────────────────────────────────
   DMA 수신 콜백 - Idle 감지 시 호출됨
   수신된 바이트를 상태머신에 1바이트씩 넘김
───────────────────────────────────────── */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == uart->Instance)
    {
        for (int i = 0; i < Size; i++)
        {
            UART_RxCallback(rx_buf[i]);
        }
        // 다음 수신을 위해 DMA 재시작
        HAL_UARTEx_ReceiveToIdle_DMA(uart, rx_buf, sizeof(rx_buf));
    }
}

/* ─────────────────────────────────────────
   상태머신 - 1바이트씩 파싱
   패킷 완성 및 CRC 검증 시 pktReady 플래그 설정
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

/* ─────────────────────────────────────────
   송신 - 패킷 조립 후 전송
───────────────────────────────────────── */
static void SendPacket(uint8_t cmd, uint8_t *data, uint8_t len)
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

void UART_SendACK(uint8_t cmd)
{
    SendPacket(CMD_ACK, &cmd, 1);
}

void UART_SendNACK(uint8_t cmd, uint8_t err)
{
    uint8_t data[2] = {cmd, err};
    SendPacket(CMD_NACK, data, 2);
}

void UART_SendSensorResp(uint8_t cmd, uint8_t *data, uint8_t len)
{
    // RESP packet: [cmd 1B][data nB]
    uint8_t buf[PKT_MAX_DATA_LEN];
    buf[0] = cmd;
    for (int i = 0; i < len; i++)
        buf[i + 1] = data[i];
    SendPacket(CMD_RESP_SENSOR, buf, len + 1);
}

/* ─────────────────────────────────────────
   메인 루프에서 주기적으로 호출
   pktReady 플래그 확인 후 CMD 처리
───────────────────────────────────────── */
void UART_Handler_Process(void)
{
    if (!pktReady)
        return;
    pktReady = 0;

    Packet_t pkt = pendingPkt;

    switch (pkt.cmd) {
        case CMD_GET_CO: {
        	// 1. 센서값 읽기 (float 형태)
        	float co_val = MQ7_ReadCO(&hadc1, 0.1f);

        	// 2. 소수점 두 자리를 포함하기 위해 100을 곱해 정수로 변환
        	uint16_t send_val = (uint16_t)(co_val * 100);

            // 3. 상위/하위 바이트로 분리하여 응답 배열 생성
       	    uint8_t resp[3];
       	    resp[0] = CMD_GET_CO;             // 어떤 데이터인지 구분자
       	    resp[1] = (send_val >> 8) & 0xFF;  // 상위 바이트 (MSB)
       	    resp[2] = send_val & 0xFF;         // 하위 바이트 (LSB)

       	    // 4. 라즈베리파이로 패킷 전송
       	    UART_SendSensorResp(CMD_RESP_SENSOR, resp, 3);
            break;
        }
        case CMD_GET_CO2: {
        	// 1. 센서값 읽기 (float 형태)
       	    float co2_val = MQ135_ReadCO2(&hadc1, 0.1f);

       	    // 2. 100을 곱해 정수화
       	    uint16_t send_val = (uint16_t)(co2_val * 100);

       	    uint8_t resp[3];
       	    resp[0] = CMD_GET_CO2;
       	    resp[1] = (send_val >> 8) & 0xFF;
       	    resp[2] = send_val & 0xFF;

       	    UART_SendSensorResp(CMD_RESP_SENSOR, resp, 3);
        	break;
    }
    case CMD_GET_TEMP_HUM:
    {
        // RX payload format (LEN=4): [temp_hi][temp_lo][hum_hi][hum_lo], x10 scale
        if (pkt.len != 4U)
        {
            UART_SendNACK(pkt.cmd, ERR_INVALID_DATA);
            break;
        }

        int16_t temp_deci = (int16_t)(((uint16_t)pkt.data[0] << 8) | pkt.data[1]);
        int16_t hum_deci = (int16_t)(((uint16_t)pkt.data[2] << 8) | pkt.data[3]);

        Data_Manager_SetTempHumValues((float)temp_deci / 100.0f, (float)hum_deci / 100.0f);
        printf("[UART] Temp/Hum update: T=%.1fC H=%.1f%%\\r\\n",
               (float)temp_deci / 100.0f, (float)hum_deci / 100.0f);
        UART_SendACK(pkt.cmd);
        break;
    }
    case CMD_SET_LED:
        if (pkt.len != 8U)
        {
            UART_SendNACK(pkt.cmd, ERR_INVALID_DATA);
            break;
        }
        for (int i = 0; i < 8; i++)
        {
            if (pkt.data[i] > 2U)
            {
                UART_SendNACK(pkt.cmd, ERR_INVALID_DATA);
                return;
            }
        }
        MatrixRun_SetCongestionBulk(pkt.data);
        printf("[UART] Bulk congestion update: P1=%d, P8=%d\r\n",
               pkt.data[0], pkt.data[7]);
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
        Audio_StartWav(filename);
        UART_SendNACK(CMD_ACK, 0);
        break;

    case CMD_GET_WAVS:
        printf("[RECV] GET_WAVS\r\n");

        uint8_t data[255];
        uint8_t len;

//        sd_print_files();
        sd_read_files("/", data, &len);
        //			if (sd_read_files("/", data, &len) != FR_OK) return;
        SendPacket(CMD_RESP_WAVS, data, len);

        break;

    default:
        UART_SendNACK(pkt.cmd, ERR_INVALID_CMD);
        break;
    }
}
