#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include <stdint.h>

#define PKT_STX     0xAA
#define PKT_ETX     0x55

// CMD 정의
#define CMD_GET_CO          0x01
#define CMD_GET_CO2         0x02
#define CMD_GET_TEMP_HUM    0x03
#define CMD_SET_LED         0x10
#define CMD_SET_AUDIO       0x20
#define CMD_RESP_SENSOR     0x80
#define CMD_ACK             0xF0
#define CMD_NACK            0xF1

// NACK 에러 코드
#define ERR_INVALID_CMD     0x01
#define ERR_INVALID_DATA    0x02
#define ERR_DEVICE_BUSY     0x03

#define PKT_MAX_DATA_LEN    16

typedef struct {
    uint8_t cmd;
    uint8_t len;
    uint8_t data[PKT_MAX_DATA_LEN];
    uint8_t crc;
} Packet_t;

#endif
