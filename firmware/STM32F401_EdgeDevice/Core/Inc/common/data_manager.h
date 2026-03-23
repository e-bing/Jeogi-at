#ifndef COMMON_DATA_MANAGER_H
#define COMMON_DATA_MANAGER_H

#include "main.h"

typedef struct {
    double co_val;       // CO concentration
    double co2_val;      // CO2 concentration
    double temp_val;     // Temperature value
    double hum_val;      // Humidity value
    int target_num;      // target platform number
    uint8_t conn_status; // connection status (0: disconnected, 1: connected)
} DB_Data_t;

extern DB_Data_t g_db_data;

void Data_Manager_Init(void);
void Data_Manager_Update(char* raw_string);
void Data_Manager_SetSensorValues(float co, float co2);
void Data_Manager_SetTempHumValues(float temp, float hum);

#endif /* COMMON_DATA_MANAGER_H */
