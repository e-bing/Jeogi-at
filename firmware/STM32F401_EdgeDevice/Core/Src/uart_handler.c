#include "uart_handler.h"
#include "uart_protocol.h"
#include "usart.h"      // CubeMX 생성 파일 (huart2 등)
// 실제 장치 헤더로 교체
#include <drivers/audio/audio_out.h>
// #include "led_panel.h"
// #include "mq7.h"
// #include "mq135.h"
// #include "sht20.h"

/* ─────────────────────────────────────────
   내부 변수
───────────────────────────────────────── */

static UART_HandleTypeDef *uart;            // Init 시 등록된 UART 핸들
static uint8_t rx_buf[64];                  // DMA 수신 버퍼

// 수신 상태머신 상태 정의
typedef enum {
    STATE_WAIT_STX,   // STX 대기
    STATE_RECV_CMD,   // CMD 수신
    STATE_RECV_LEN,   // LEN 수신
    STATE_RECV_DATA,  // DATA 수신
    STATE_RECV_CRC,   // CRC 수신
    STATE_WAIT_ETX    // ETX 대기 및 패킷 검증
} RxState_t;

static RxState_t        rxState  = STATE_WAIT_STX;  // 현재 상태머신 상태
static Packet_t         rxPkt;                       // 수신 중인 패킷
static uint8_t          dataIdx;                     // DATA 필드 인덱스
static volatile uint8_t pktReady = 0;               // 패킷 수신 완료 플래그
static Packet_t         pendingPkt;                  // 처리 대기 중인 패킷

/* ─────────────────────────────────────────
   CRC 계산 (CMD ^ LEN ^ DATA 바이트들 XOR)
───────────────────────────────────────── */
static uint8_t CalcCRC(Packet_t *pkt)
{
    uint8_t crc = pkt->cmd ^ pkt->len;
    for (int i = 0; i < pkt->len; i++) crc ^= pkt->data[i];
    crc ^= PKT_ETX;   // ETX 포함
    return crc;
}

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
    if (huart->Instance == uart->Instance) {
        for (int i = 0; i < Size; i++) {
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

	static int cnt = 0;
	if (cnt++ < 30) {
	  printf("rx byte = 0x%02X, state=%d\r\n", byte, rxState);
	}


    switch (rxState) {
        case STATE_WAIT_STX:
            if (byte == PKT_STX) rxState = STATE_RECV_CMD;
            break;

        case STATE_RECV_CMD:
            rxPkt.cmd = byte;
            rxState = STATE_RECV_LEN;
            break;

        case STATE_RECV_LEN:
            rxPkt.len = byte;
            dataIdx = 0;
            // DATA가 없으면 CRC로 바로 이동
            rxState = (byte > 0) ? STATE_RECV_DATA : STATE_RECV_CRC;
            break;

        case STATE_RECV_DATA:
            if (dataIdx < PKT_MAX_DATA_LEN)
                rxPkt.data[dataIdx++] = byte;
            if (dataIdx >= rxPkt.len)
                rxState = STATE_RECV_CRC;
            break;

        case STATE_RECV_CRC:
            rxPkt.crc = byte;
            rxState = STATE_WAIT_ETX;
            break;

        case STATE_WAIT_ETX:
        	  if (byte == PKT_ETX) {
        	    uint8_t c = CalcCRC(&rxPkt);
        	    if (c == rxPkt.crc) {
        	      pktReady = 1;
        	      pendingPkt = rxPkt;
        	      printf("OK cmd=%02X len=%d\r\n", rxPkt.cmd, rxPkt.len);
        	    } else {
        	      printf("CRC FAIL calc=%02X rx=%02X cmd=%02X len=%d\r\n",
        	             c, rxPkt.crc, rxPkt.cmd, rxPkt.len);
        	    }
        	  } else {
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
    for (int i = 0; i < len; i++) buf[idx++] = data[i];

    // CRC: STX 제외 나머지 XOR => cmd ^ len ^ data... ^ ETX
    uint8_t crc = cmd ^ len ^ PKT_ETX;
    for (int i = 0; i < len; i++) crc ^= data[i];

    buf[idx++] = crc;
    buf[idx++] = PKT_ETX;

    HAL_UART_Transmit(uart, buf, idx, 100);
}

void UART_SendACK(uint8_t cmd) {
    SendPacket(CMD_ACK, &cmd, 1);
}

void UART_SendNACK(uint8_t cmd, uint8_t err) {
    uint8_t data[2] = {cmd, err};
    SendPacket(CMD_NACK, data, 2);
}

void UART_SendSensorResp(uint8_t cmd, uint8_t *data, uint8_t len) {
    // RESP 패킷: [cmd 1B][data nB]
    uint8_t buf[PKT_MAX_DATA_LEN];
    buf[0] = cmd;
    for (int i = 0; i < len; i++) buf[i + 1] = data[i];
    SendPacket(CMD_RESP_SENSOR, buf, len + 1);
}

/* ─────────────────────────────────────────
   메인 루프에서 주기적으로 호출
   pktReady 플래그 확인 후 CMD 처리
───────────────────────────────────────── */
void UART_Handler_Process(void)
{
    if (!pktReady) return;
    pktReady = 0;

    Packet_t pkt = pendingPkt;

    switch (pkt.cmd) {
        case CMD_GET_CO: {
        	// 일산화탄소 센서 읽기 추가
//            uint16_t ppm = Device_ReadCO();
//            uint8_t resp[2] = {ppm >> 8, ppm & 0xFF};
//            UART_SendSensorResp(CMD_GET_CO, resp, 2);
            break;
        }
        case CMD_GET_CO2: {
        	// 이산화탄소 센서 읽기 추가
//            uint16_t ppm = Device_ReadCO2();
//            uint8_t resp[2] = {ppm >> 8, ppm & 0xFF};
//            UART_SendSensorResp(CMD_GET_CO2, resp, 2);
            break;
        }
        case CMD_GET_TEMP_HUM: {
        	// 온습도 센서 읽기 추가
//            uint16_t temp, hum;
//            Device_ReadTempHum(&temp, &hum);
//            uint8_t resp[4] = {temp >> 8, temp & 0xFF, hum >> 8, hum & 0xFF};
//            UART_SendSensorResp(CMD_GET_TEMP_HUM, resp, 4);
            break;
        }
        case CMD_SET_LED:
        	// LED 입력 추가
//            if (pkt.len < 1) { UART_SendNACK(pkt.cmd, ERR_INVALID_DATA); break; }
//            Device_SetLED(pkt.data[0]);
//            UART_SendACK(pkt.cmd);
            break;

        case CMD_SET_AUDIO:
            if (pkt.len < 1) { UART_SendNACK(pkt.cmd, ERR_INVALID_DATA); break; }

//            Audio_PlayWav("voice_1.wav");
//            Device_SetAudio(pkt.data[0]);
            UART_SendACK(pkt.cmd);
            break;

        default:
            UART_SendNACK(pkt.cmd, ERR_INVALID_CMD);
            break;
    }
}
