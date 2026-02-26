#ifndef SD_SPI_H
#define SD_SPI_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* 초기화 */
uint8_t sd_init(void);

/* 섹터 읽기 */
int sd_read_multi(uint32_t lba, uint8_t *buf, uint32_t count);

#endif /* SD_SPI_H */
