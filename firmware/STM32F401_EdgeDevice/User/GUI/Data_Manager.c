#include "Data_Manager.h"
#include <stdio.h>
#include <string.h>

// 실제 데이터가 저장되는 메모리 공간
DB_Data_t g_db_data;

/**
 * @brief 데이터 초기화 (부팅 시 호출)
 */
void Data_Manager_Init(void) {
    g_db_data.co2_val = 0.0;
    g_db_data.target_num = 0;
}

/**
 * @brief DB/API로부터 받은 문자열을 숫자로 변환
 * @param raw_data 입력 문자열 (예: "330.57,8")
 */
void Data_Manager_Update(char* raw_data) {
    if (raw_data == NULL) return;

    float temp_co2;
    int temp_num;

    // "실수,정수" 형태의 문자열을 파싱
   //  성공적으로 2개의 항목을 읽었을 때만 데이터를 업데이트합니다.
    if (sscanf(raw_data, "%f,%d", &temp_co2, &temp_num) == 2) {
        g_db_data.co2_val = (double)temp_co2;
        g_db_data.target_num = temp_num;

    }
}
