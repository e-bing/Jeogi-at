/*
 * sd_spi.h
 *
 *  Created on: 2026. 2. 27.
 *      Author: Taewoo Yun
 */

#ifndef INC_DRIVERS_STORAGE_SD_SPI_H_
#define INC_DRIVERS_STORAGE_SD_SPI_H_

void audio_out_init();
void audio_out_start();
void audio_out_stop();
void audio_out_submit_buffer();
void audio_out_poll_events();

#endif /* INC_DRIVERS_AUDIO_AUDIO_OUT_H_ */

#ifndef SD_SPI_H
#define SD_SPI_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* 초기화 */
uint8_t sd_init(void);

/* 섹터 읽기 */
int sd_read_multi(uint32_t lba, uint8_t *buf, uint32_t count);

#endif /* INC_DRIVERS_STORAGE_SD_SPI_H_ */
