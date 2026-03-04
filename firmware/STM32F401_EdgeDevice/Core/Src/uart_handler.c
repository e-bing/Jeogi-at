#include "uart_handler.h"
#include "uart_protocol.h"
#include "usart.h"      // CubeMX generated file (huart2, etc.)
#include "i2s_audio.h"
// #include "led_panel.h"
// #include "mq7.h"
// #include "mq135.h"
// #include "sht20.h"

/* ─────────────────────────────────────────
   Internal variables
───────────────────────────────────────── */

static UART_HandleTypeDef *uart;            // UART handle registered at init
static uint8_t rx_buf[64];                  // DMA RX buffer

// RX state machine states
typedef enum {
    STATE_WAIT_STX,   // Wait STX
    STATE_RECV_CMD,   // Receive CMD
    STATE_RECV_LEN,   // Receive LEN
    STATE_RECV_DATA,  // Receive DATA
    STATE_RECV_CRC,   // Receive CRC
    STATE_WAIT_ETX    // Wait ETX and validate packet
} RxState_t;

static RxState_t        rxState  = STATE_WAIT_STX;  // Current RX parser state
static Packet_t         rxPkt;                       // Packet being parsed
static uint8_t          dataIdx;                     // DATA field index
static volatile uint8_t pktReady = 0;               // Packet ready flag
static Packet_t         pendingPkt;                  // Pending packet for processing

/* ─────────────────────────────────────────
   CRC: CMD ^ LEN ^ DATA bytes ^ ETX
───────────────────────────────────────── */
static uint8_t CalcCRC(Packet_t *pkt)
{
    uint8_t crc = pkt->cmd ^ pkt->len;
    for (int i = 0; i < pkt->len; i++) crc ^= pkt->data[i];
    crc ^= PKT_ETX;   // include ETX
    return crc;
}

/* ─────────────────────────────────────────
   Init - start DMA RX
───────────────────────────────────────── */
void UART_CMD_Init(UART_HandleTypeDef *huart)
{
    uart = huart; // use configured uart
    HAL_UARTEx_ReceiveToIdle_DMA(uart, rx_buf, sizeof(rx_buf));
    printf("__uart__ __init__\r\n");
}

/* ─────────────────────────────────────────
   DMA RX callback (called on IDLE)
   Feed received bytes into parser
───────────────────────────────────────── */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == uart->Instance) {
        for (int i = 0; i < Size; i++) {
            UART_RxCallback(rx_buf[i]);
        }
        // Rearm DMA for next RX
        HAL_UARTEx_ReceiveToIdle_DMA(uart, rx_buf, sizeof(rx_buf));
    }
}

/* ─────────────────────────────────────────
   Byte-wise parser state machine
   Set pktReady when full packet + CRC is valid
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
            // No DATA -> go directly to CRC
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
   TX - build and send packet
───────────────────────────────────────── */
static void SendPacket(uint8_t cmd, uint8_t *data, uint8_t len)
{
    uint8_t buf[PKT_MAX_DATA_LEN + 6];
    uint8_t idx = 0;

    buf[idx++] = PKT_STX;
    buf[idx++] = cmd;
    buf[idx++] = len;
    for (int i = 0; i < len; i++) buf[idx++] = data[i];

    // CRC: cmd ^ len ^ data... ^ ETX
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
    // RESP packet: [cmd 1B][data nB]
    uint8_t buf[PKT_MAX_DATA_LEN];
    buf[0] = cmd;
    for (int i = 0; i < len; i++) buf[i + 1] = data[i];
    SendPacket(CMD_RESP_SENSOR, buf, len + 1);
}

/* ─────────────────────────────────────────
   Called periodically in main loop
   Check pktReady and dispatch CMD
───────────────────────────────────────── */
void UART_Handler_Process(void)
{
    if (!pktReady) return;
    pktReady = 0;

    Packet_t pkt = pendingPkt;

    switch (pkt.cmd) {
        case CMD_GET_CO: {
            // TODO: add CO sensor read
//            uint16_t ppm = Device_ReadCO();
//            uint8_t resp[2] = {ppm >> 8, ppm & 0xFF};
//            UART_SendSensorResp(CMD_GET_CO, resp, 2);
            break;
        }
        case CMD_GET_CO2: {
            // TODO: add CO2 sensor read
//            uint16_t ppm = Device_ReadCO2();
//            uint8_t resp[2] = {ppm >> 8, ppm & 0xFF};
//            UART_SendSensorResp(CMD_GET_CO2, resp, 2);
            break;
        }
        case CMD_GET_TEMP_HUM: {
            // TODO: add temp/hum sensor read
//            uint16_t temp, hum;
//            Device_ReadTempHum(&temp, &hum);
//            uint8_t resp[4] = {temp >> 8, temp & 0xFF, hum >> 8, hum & 0xFF};
//            UART_SendSensorResp(CMD_GET_TEMP_HUM, resp, 4);
            break;
        }
        case CMD_SET_LED:
            if (pkt.len != 8U) {
                UART_SendNACK(pkt.cmd, ERR_INVALID_DATA);
                break;
            }
            for (int i = 0; i < 8; i++) {
                if (pkt.data[i] > 2U) {
                    UART_SendNACK(pkt.cmd, ERR_INVALID_DATA);
                    return;
                }
            }
            MatrixRun_SetCongestionBulk(pkt.data);
            printf("[UART] Bulk congestion update: P1=%d, P8=%d\r\n",
                   pkt.data[0], pkt.data[7]);
            UART_SendACK(pkt.cmd);
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
