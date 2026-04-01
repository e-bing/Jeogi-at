#ifndef DRIVERS_STORAGE_SD_SPI_H
#define DRIVERS_STORAGE_SD_SPI_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

uint8_t sd_init(void);
int sd_read_block(uint32_t lba, uint8_t *buf);
int sd_read_multi(uint32_t lba, uint8_t *buf, uint32_t count);

#endif /* DRIVERS_STORAGE_SD_SPI_H */
