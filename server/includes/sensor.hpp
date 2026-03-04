#ifndef SENSOR_HPP
#define SENSOR_HPP

#include <mariadb/mysql.h>

/**
 * @brief aboy로부터 MQTT로 센서값을 수신해서 DB에 저장합니다.
 * @param conn DB 연결 핸들
 */
void receive_sensor_data(MYSQL* conn);

bool get_last_temp_humi(float& temp, float& humi);

#endif // SENSOR_H
