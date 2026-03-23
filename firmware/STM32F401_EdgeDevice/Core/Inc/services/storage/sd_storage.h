/*
 * sd_storage.h
 *
 *  Created on: 2026. 2. 27.
 *      Author: Taewoo Yun
 */

#ifndef SERVICES_STORAGE_SD_STORAGE_H
#define SERVICES_STORAGE_SD_STORAGE_H

#include "fatfs.h"

void sd_print_files(void);
FRESULT sd_read_files(const char *path, uint8_t *data, uint8_t *len);


#endif /* SERVICES_STORAGE_SD_STORAGE_H */
