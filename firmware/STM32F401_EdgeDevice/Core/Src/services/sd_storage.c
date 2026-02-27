#include <stdio.h>
#include "spi.h"
#include "fatfs.h"

void sd_read_files()
{
    DIR dir;
    FILINFO fno;

    if (f_opendir(&dir, "/") == FR_OK) {
        while (1) {
            if (f_readdir(&dir, &fno) != FR_OK) break;
            if (fno.fname[0] == 0) break;
            printf("%s\r\n", fno.fname);
        }
    }
}
