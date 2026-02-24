#ifndef __DATA_MANAGER_H
#define __DATA_MANAGER_H

#include "main.h"

// DB에서 받아올 실시간 데이터 구조체
typedef struct {
    double co2_val;      // CO2 농도
    int target_num;      // 경고 수치나 목표 번호
    uint8_t conn_status; // 연결 상태 (0: 끊김, 1: 연결됨)
} DB_Data_t;

// 전역 변수 선언 (외부에서 읽기 가능)
extern DB_Data_t g_db_data;

void Data_Manager_Init(void);
void Data_Manager_Update(char* raw_string); // 수신된 데이터를 파싱하여 업데이트

#endif
