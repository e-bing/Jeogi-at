#ifndef __SPI_SDCARD_H__
#define __SPI_SDCARD_H__

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* 초기화 */
uint8_t sd_init(void);

/* 섹터 읽기 */
uint8_t sd_read_block(uint32_t lba, uint8_t *buf);

#endif
