#include "spi_sdcard.h"
#include "spi.h"
#include "gpio.h"


/* ================= CS CONTROL ================= */

#define SD_CS_LOW()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET)
#define SD_CS_HIGH()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET)


/* ================= SPI TXRX ================= */

static uint8_t spi_txrx(uint8_t data)
{
    uint8_t rx;
    HAL_SPI_TransmitReceive(&hspi2, &data, &rx, 1, HAL_MAX_DELAY);
    return rx;
}


/* ================= DUMMY CLOCK ================= */

static void sd_dummy_clock(void)
{
    SD_CS_HIGH();

    for(int i=0;i<10;i++)
        spi_txrx(0xFF);   // 80 clocks
}


/* ================= SD COMMAND ================= */

static uint8_t sd_cmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t r;

    spi_txrx(0x40 | cmd);

    spi_txrx(arg >> 24);
    spi_txrx(arg >> 16);
    spi_txrx(arg >> 8);
    spi_txrx(arg);

    spi_txrx(crc);

    /* wait R1 */
    for(int i=0;i<100;i++)
    {
        r = spi_txrx(0xFF);
        if(r != 0xFF) break;
    }

    return r;
}


/* ================= SD INIT ================= */

uint8_t sd_init(void)
{
    uint8_t r;
    uint8_t r7[4];


    /* 80 dummy clocks */
    sd_dummy_clock();


    /* ---------- CMD0 ---------- */

    for(int i=0;i<50;i++)
    {
        SD_CS_LOW();
        r = sd_cmd(0, 0, 0x95);
        SD_CS_HIGH();
        spi_txrx(0xFF);

        if(r == 0x01) break;

        HAL_Delay(10);
    }

    if(r != 0x01) return 1;


    /* ---------- CMD8 ---------- */

    SD_CS_LOW();
    r = sd_cmd(8, 0x1AA, 0x87);

    if(r == 0x01)
    {
        for(int i=0;i<4;i++)
            r7[i] = spi_txrx(0xFF);
    }

    SD_CS_HIGH();
    spi_txrx(0xFF);

    if(r != 0x01) return 2;

    if(r7[3] != 0xAA) return 3;


    /* ---------- ACMD41 ---------- */

    do
    {
        /* CMD55 */
        SD_CS_LOW();
        r = sd_cmd(55, 0, 0x01);
        SD_CS_HIGH();
        spi_txrx(0xFF);

        if(r > 0x01) continue;


        /* ACMD41 */
        SD_CS_LOW();
        r = sd_cmd(41, 0x40000000, 0x01);   // HCS
        SD_CS_HIGH();
        spi_txrx(0xFF);

    } while(r != 0x00);


    /* ---------- CMD58 (OCR check) ---------- */

    SD_CS_LOW();
    r = sd_cmd(58, 0, 0x01);

    for(int i=0;i<4;i++)
        spi_txrx(0xFF);

    SD_CS_HIGH();
    spi_txrx(0xFF);


    return 0;
}


/* ================= READ BLOCK ================= */

uint8_t sd_read_block(uint32_t lba, uint8_t *buf)
{
    uint8_t r;
    uint32_t retry;


    /* CMD17 */

    SD_CS_LOW();
    r = sd_cmd(17, lba, 0x01);

    if(r != 0x00)
    {
        SD_CS_HIGH();
        return 1;
    }


    /* wait data token */
    retry = 0;

    do
    {
        r = spi_txrx(0xFF);
        retry++;

    } while(r != 0xFE && retry < 100000);


    if(r != 0xFE)
    {
        SD_CS_HIGH();
        return 2;
    }


    /* read 512 bytes */

    for(int i=0;i<512;i++)
        buf[i] = spi_txrx(0xFF);


    /* discard CRC */
    spi_txrx(0xFF);
    spi_txrx(0xFF);


    SD_CS_HIGH();
    spi_txrx(0xFF);


    return 0;
}
