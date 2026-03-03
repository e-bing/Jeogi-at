#include <stdio.h>
#include "spi.h"
#include "fatfs.h"
#include "uart_protocol.h"

void sd_print_files()
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

FRESULT sd_read_files(const char *path, uint8_t *data, uint8_t *len)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;

    uint8_t used = 0;

    res = f_opendir(&dir, path);
    if (res != FR_OK) {
        uint8_t err = (uint8_t)res;
//        send_frame(CMD_ERROR, &err, 1);
        return res;
    }

    while (1) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK) break;
        if (fno.fname[0] == 0) break;      // end of dir
        if (fno.fattrib & AM_DIR) continue; // 디렉토리 제외(원하면 제거)

        const char *ext = strrchr(fno.fname, '.');
        if (ext && strcmp(ext, ".wav") != 0) continue; // .wav 이외 파일 제외

        const char *name = fno.fname;      // LFN 설정에 따라 8.3일 수 있음
        uint16_t nlen = (uint16_t)strlen(name);

        // DATA는 최대 255B, 각 파일명 뒤에 '\n' 1바이트 추가
        if (used + nlen + 1 > 255) {
            // 정책: 255 넘으면 (1) 여기서 중단하고 현재까지 전송
            //      (2) 에러 보내고 실패처리
            // 여기선 (1) 중단 후 전송
            break;
        }

        memcpy(&data[used], name, nlen);
        used += nlen;
        data[used++] = '\n';
    }

    f_closedir(&dir);
    *len = used;

    // 빈 목록이어도 LEN=0으로 응답 가능
//    send_frame(CMD_FILE_LIST_RESP, data, (uint8_t)used);
    return res;
}
