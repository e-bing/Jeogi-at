/*
 * sd_storage.h
 *
 *  Created on: 2026. 2. 27.
 *      Author: Taewoo Yun
 */

#ifndef INC_SERVICES_SD_STORAGE_H_
#define INC_SERVICES_SD_STORAGE_H_

#include "fatfs.h"

void sd_print_files();
FRESULT sd_read_files(const char *path, uint8_t *data, uint8_t *len);


#endif /* INC_SERVICES_SD_STORAGE_H_ */
