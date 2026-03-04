#ifndef SHT20_H
#define SHT20_H

#include <string>

// SHT20 온습도 데이터를 저장하는 구조체
struct SHT20Data {
    float temperature;
    float humidity;
};

// 함수 선언
// 초기화 및 MQTT 연결을 담당한다.
int init_sht20();

// 열려있는 파일 디스크립터를 닫는다.
void close_sht20(int fd);

// 메인 루프를 돌며 데이터를 읽고 전송한다.
void run_sht20_monitor(int fd);

#endif // SHT20_H
