#include "spi_sdcard.h"
#include "spi.h"
#include "gpio.h"

#define SD_CS_LOW()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET)
#define SD_CS_HIGH()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET)

static uint8_t spi_txrx(uint8_t data)
{
    uint8_t rx;
    HAL_SPI_TransmitReceive(&hspi2, &data, &rx, 1, HAL_MAX_DELAY);
    return rx;
}

void sd_send_dummy(void)
{
    SD_CS_HIGH();

    for(int i=0;i<10;i++)
        spi_txrx(0xFF);   // 80클럭
}

uint8_t sd_cmd0(void)
{
    uint8_t r;

    SD_CS_LOW();

    spi_txrx(0x40);   // CMD0
    spi_txrx(0x00);
    spi_txrx(0x00);
    spi_txrx(0x00);
    spi_txrx(0x00);
    spi_txrx(0x95);   // CRC

    // 응답 대기
    for(int i=0;i<100;i++)
    {
        r = spi_txrx(0xFF);
        if(r != 0xFF)
            break;
    }

    SD_CS_HIGH();
    spi_txrx(0xFF);   // trailing clock

    return r;
}

uint8_t sd_cmd8(uint8_t *buf)
{
    uint8_t r;

    SD_CS_LOW();

    spi_txrx(0x48);   // CMD8
    spi_txrx(0x00);
    spi_txrx(0x00);
    spi_txrx(0x01);
    spi_txrx(0xAA);
    spi_txrx(0x87);   // CRC

    // R1 응답
    for(int i=0;i<100;i++)
    {
        r = spi_txrx(0xFF);
        if(r != 0xFF)
            break;
    }

    // R7 응답 나머지 4바이트
    for(int i=0;i<4;i++)
        buf[i] = spi_txrx(0xFF);

    SD_CS_HIGH();
    spi_txrx(0xFF);

    return r;
}

uint8_t sd_acmd41(void)
{
    uint8_t r;

    SD_CS_LOW();

    spi_txrx(0x69); // ACMD41
    spi_txrx(0x40); // HCS bit
    spi_txrx(0x00);
    spi_txrx(0x00);
    spi_txrx(0x00);
    spi_txrx(0x01);

    for(int i=0;i<100;i++){
        r = spi_txrx(0xFF);
        if(r != 0xFF) break;
    }

    SD_CS_HIGH();
    spi_txrx(0xFF);

    return r;
}

uint8_t sd_cmd55(void)
{
    uint8_t r;

    SD_CS_LOW();

    spi_txrx(0x77); // CMD55
    for(int i=0;i<4;i++) spi_txrx(0x00);
    spi_txrx(0x01);

    for(int i=0;i<100;i++){
        r = spi_txrx(0xFF);
        if(r != 0xFF) break;
    }

    SD_CS_HIGH();
    spi_txrx(0xFF);

    return r;
}
