#include "Data_Manager.h"
#include <stdio.h>

DB_Data_t g_db_data;

void Data_Manager_Init(void)
{
    g_db_data.co_val = 0.0;
    g_db_data.co2_val = 0.0;
    g_db_data.target_num = 0;
    g_db_data.conn_status = 0;
}

void Data_Manager_SetSensorValues(float co, float co2)
{
    g_db_data.co_val = (double)co;
    g_db_data.co2_val = (double)co2;
}

void Data_Manager_Update(char* raw_data)
{
    if (raw_data == NULL) {
        return;
    }

    float temp_co2 = 0.0f;
    int temp_num = 0;

    // Legacy parser path: "co2,target_num"
    if (sscanf(raw_data, "%f,%d", &temp_co2, &temp_num) == 2) {
        g_db_data.co2_val = (double)temp_co2;
        g_db_data.target_num = temp_num;
    }
}
