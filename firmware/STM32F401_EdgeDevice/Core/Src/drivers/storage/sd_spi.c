#include <drivers/storage/sd_spi.h>
#include "spi.h"
#include "gpio.h"
#include <string.h>

/* ================= CS CONTROL ================= */

#define SD_CS_LOW()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET)
#define SD_CS_HIGH()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET)

/* ================= CMD ================= */
#ifndef CMD0
#define CMD0   0
#define CMD8   8
#define CMD12  12
#define CMD17  17
#define CMD18  18
#define CMD55  55
#define CMD58  58
#define ACMD41 41
#endif

/* ================= 카드 타입 ================= */
static uint8_t g_sd_is_sdhc = 0;  // 1=SDHC/SDXC(LBA), 0=SDSC(byte addressing)

/* ================= SPI TXRX (1 byte) ================= */

static uint8_t spi_txrx_u8(uint8_t data)
{
    uint8_t rx;
    HAL_SPI_TransmitReceive(&hspi2, &data, &rx, 1, HAL_MAX_DELAY);
    return rx;
}

/* ================= SPI READ/WRITE BUFFER ================= */
/* 1바이트 HAL 호출 제거: 이게 속도 개선의 핵심 */

static void spi_read_buf(uint8_t *dst, uint32_t len)
{
    static uint8_t dummy[512];
    /* len이 512 초과면 512씩 쪼개서 */
    while (len)
    {
        uint32_t chunk = (len > sizeof(dummy)) ? sizeof(dummy) : len;
        memset(dummy, 0xFF, chunk);
        HAL_SPI_TransmitReceive(&hspi2, dummy, dst, chunk, HAL_MAX_DELAY);
        dst += chunk;
        len -= chunk;
    }
}

static void spi_write_buf(const uint8_t *src, uint32_t len)
{
    HAL_SPI_Transmit(&hspi2, (uint8_t*)src, len, HAL_MAX_DELAY);
}

/* ================= DUMMY CLOCK ================= */

static void sd_dummy_clock(void)
{
    SD_CS_HIGH();
    for (int i = 0; i < 10; i++) spi_txrx_u8(0xFF); // 80 clocks
}

/* ================= WAIT TOKEN / READY ================= */

static int sd_wait_token(uint8_t token, uint32_t timeout_ms)
{
    uint8_t r;
    uint32_t t0 = HAL_GetTick();
    do {
        r = spi_txrx_u8(0xFF);
        if (r == token) return 0;
    } while (HAL_GetTick() - t0 < timeout_ms);
    return -1;
}

static int sd_wait_ready(uint32_t timeout_ms)
{
    uint32_t t0 = HAL_GetTick();
    while (spi_txrx_u8(0xFF) != 0xFF) {
        if (HAL_GetTick() - t0 > timeout_ms) return -1;
    }
    return 0;
}

/* ================= SD COMMAND (R1) ================= */
/* 기존 "100번 루프" 대신 시간 기반 타임아웃. */
static uint8_t sd_cmd_r1(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t pkt[6];
    pkt[0] = 0x40 | cmd;
    pkt[1] = (uint8_t)(arg >> 24);
    pkt[2] = (uint8_t)(arg >> 16);
    pkt[3] = (uint8_t)(arg >> 8);
    pkt[4] = (uint8_t)(arg);
    pkt[5] = crc;

    spi_write_buf(pkt, 6);

    /* wait R1 (최대 10ms) */
    uint8_t r;
    uint32_t t0 = HAL_GetTick();
    do {
        r = spi_txrx_u8(0xFF);
        if ((r & 0x80) == 0) return r;
    } while (HAL_GetTick() - t0 < 10);

    return 0xFF;
}

/* ================= CMD12 (R1b) ================= */
/* CMD12는 예외: dummy 1바이트 + busy 해제까지 기다려야 함 */
static int sd_send_cmd12(void)
{
    uint8_t r;

    spi_txrx_u8(0xFF); // extra dummy byte

    uint8_t pkt[6] = { (uint8_t)(0x40 | CMD12), 0,0,0,0, 0x01 };
    spi_write_buf(pkt, 6);

    /* R1b */
    uint32_t t0 = HAL_GetTick();
    do {
        r = spi_txrx_u8(0xFF);
        if ((r & 0x80) == 0) break;
    } while (HAL_GetTick() - t0 < 10);

    /* busy 해제 */
    if (sd_wait_ready(50) != 0) return -1;

    return 0;
}

/* ================= SD INIT ================= */

uint8_t sd_init(void)
{
    uint8_t r;
    uint8_t r7[4] = {0};
    uint8_t ocr[4] = {0};

    g_sd_is_sdhc = 0;

    sd_dummy_clock();

    /* ---------- CMD0 ---------- */
    for (int i = 0; i < 50; i++)
    {
        SD_CS_LOW();
        r = sd_cmd_r1(CMD0, 0, 0x95);
        SD_CS_HIGH();
        spi_txrx_u8(0xFF);

        if (r == 0x01) break;
        HAL_Delay(10);
    }
    if (r != 0x01) return 1;

    /* ---------- CMD8 ---------- */
    SD_CS_LOW();
    r = sd_cmd_r1(CMD8, 0x1AA, 0x87);
    if (r == 0x01) {
        spi_read_buf(r7, 4);
    }
    SD_CS_HIGH();
    spi_txrx_u8(0xFF);

    if (r != 0x01) return 2;
    if (r7[3] != 0xAA) return 3;

    /* ---------- ACMD41 loop ---------- */
    do
    {
        SD_CS_LOW();
        r = sd_cmd_r1(CMD55, 0, 0x01);
        SD_CS_HIGH();
        spi_txrx_u8(0xFF);
        if (r > 0x01) continue;

        SD_CS_LOW();
        r = sd_cmd_r1(ACMD41, 0x40000000, 0x01); // HCS
        SD_CS_HIGH();
        spi_txrx_u8(0xFF);

    } while (r != 0x00);

    /* ---------- CMD58 (OCR) ---------- */
    SD_CS_LOW();
    r = sd_cmd_r1(CMD58, 0, 0x01);
    spi_read_buf(ocr, 4);
    SD_CS_HIGH();
    spi_txrx_u8(0xFF);

    if (r != 0x00) return 4;

    /* OCR[0] bit6 = CCS (SDHC/SDXC) */
    if (ocr[0] & 0x40) g_sd_is_sdhc = 1;

    return 0;
}

/* ================= Address helper ================= */
static inline uint32_t sd_addr(uint32_t lba)
{
    /* SDHC: lba 그대로 / SDSC: byte address */
    return g_sd_is_sdhc ? lba : (lba * 512UL);
}

/* ================= READ SINGLE (CMD17) ================= */

int sd_read_block(uint32_t lba, uint8_t *buf)
{
    uint8_t r;

    SD_CS_LOW();

    r = sd_cmd_r1(CMD17, sd_addr(lba), 0x01);
    if (r != 0x00) {
        SD_CS_HIGH();
        spi_txrx_u8(0xFF);
        return 1;
    }

    /* data token 0xFE */
    if (sd_wait_token(0xFE, 50) != 0) {
        SD_CS_HIGH();
        spi_txrx_u8(0xFF);
        return 2;
    }

    /* 512 bytes + CRC(2) */
    spi_read_buf(buf, 512);
    uint8_t crc[2];
    spi_read_buf(crc, 2);

    SD_CS_HIGH();
    spi_txrx_u8(0xFF);

    return 0;
}

/* ================= READ MULTI (CMD18) ================= */

int sd_read_multi(uint32_t lba, uint8_t *buf, uint32_t count)
{
	if(count <= 1) return sd_read_block(lba, buf);

    uint8_t r;

    SD_CS_LOW();

    r = sd_cmd_r1(CMD18, sd_addr(lba), 0x01);
    if (r != 0x00) {
        SD_CS_HIGH();
        spi_txrx_u8(0xFF);
        return -1;
    }

    for (uint32_t i = 0; i < count; i++)
    {
        if (sd_wait_token(0xFE, 50) != 0) {
            /* 멀티 중 오류면 stop 시도 */
            sd_send_cmd12();
            SD_CS_HIGH();
            spi_txrx_u8(0xFF);
            return -2;
        }

        spi_read_buf(buf, 512);
        buf += 512;

        uint8_t crc[2];
        spi_read_buf(crc, 2);
    }

    /* stop transmission */
    sd_send_cmd12();

    SD_CS_HIGH();
    spi_txrx_u8(0xFF);

    return 0;
}

/* ================= FATFS helper ================= */

