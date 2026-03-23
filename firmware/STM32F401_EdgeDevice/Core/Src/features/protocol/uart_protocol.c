#include "features/protocol/uart_protocol.h"
#include "ff.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

// FLAGS
#define FLAG_LAST (1u << 0)

/* ─────────────────────────────────────────
   CRC 계산 (CMD ^ LEN ^ DATA 바이트들 XOR)
───────────────────────────────────────── */
uint8_t Calc_CRC(Packet_t *pkt)
{
    uint8_t crc = pkt->cmd ^ pkt->len;
    for (int i = 0; i < pkt->len; i++) crc ^= pkt->data[i];
    crc ^= PKT_ETX;   // ETX 포함
    return crc;
}
