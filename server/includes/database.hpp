// database.h
#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <mariadb/mysql.h>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;
using namespace std;

/**
 * @brief config.json에서 DB 접속 정보를 읽어 연결합니다.
 *        config.json 키: db.host, db.user, db.pass, db.name
 * @return 성공 시 MYSQL 핸들, 실패 시 nullptr
 */
MYSQL *connect_db();
void close_db(MYSQL *conn);

// 센서 데이터 저장
bool save_sensor_data(MYSQL *conn, float co_value, float co2_value, float temp,
                      float humidity);
bool save_camera_stats(MYSQL* conn, const std::vector<int>& counts, const std::vector<int>& levels, const std::vector<std::string>& cam_ids);

// 실시간 데이터 조회
json get_realtime_congestion(MYSQL *conn);
json get_realtime_air_quality(MYSQL *conn);

// 통계 데이터 조회
json get_air_quality_stats(MYSQL* conn);
json get_temp_humi_stats(MYSQL* conn);
json get_passenger_flow_stats(MYSQL* conn);

#endif // DATABASE_HPP
