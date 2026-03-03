// database.h
#ifndef DATABASE_H
#define DATABASE_H

#include <mariadb/mysql.h>

#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;
using namespace std;

struct DBConfig {
  string host = "localhost";
  string user = "iam";
  string pass = "aboy";
  string db = "jeogi";  // 변경: test -> jeogi
};

// DB 연결 및 관리
MYSQL* connect_db(DBConfig config);
void close_db(MYSQL* conn);

// 센서 데이터 저장
bool save_sensor_data(MYSQL* conn, float co_value, float co2_value);

// 실시간 데이터 조회
json get_realtime_congestion(MYSQL* conn);
json get_realtime_air_quality(MYSQL* conn);

// 통계 데이터 조회
json get_air_quality_stats(MYSQL* conn, string cam_id);
json get_passenger_flow_stats(MYSQL* conn, string cam_id);

#endif  // DATABASE_H
