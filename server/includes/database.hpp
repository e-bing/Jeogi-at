// database.h
#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <mariadb/mysql.h>

#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;
using namespace std;

<<<<<<< HEAD
/**
 * @brief config.json에서 DB 접속 정보를 읽어 연결합니다.
 *        config.json 키: db.host, db.user, db.pass, db.name
 * @return 성공 시 MYSQL 핸들, 실패 시 nullptr
 */
MYSQL* connect_db();
void   close_db(MYSQL* conn);
=======
struct DBConfig {
  string host = "localhost";
  string user = "iam";
  string pass = "aboy";
  string db = "jeogi";
};

// DB 연결 및 관리
MYSQL* connect_db(DBConfig config);
void close_db(MYSQL* conn);
>>>>>>> 46572c401267356c78a242acd77e08c5cbb66f29

// 센서 데이터 저장
bool save_sensor_data(MYSQL* conn, float co_value, float co2_value, float temp,
                      float humidity);

// 실시간 데이터 조회
json get_realtime_congestion(MYSQL* conn);
json get_realtime_air_quality(MYSQL* conn);

// 통계 데이터 조회
json get_air_quality_stats(MYSQL* conn, string cam_id);
json get_passenger_flow_stats(MYSQL* conn, string cam_id);

<<<<<<< HEAD
#endif // DATABASE_HPP
=======
#endif  // DATABASE_H
>>>>>>> 46572c401267356c78a242acd77e08c5cbb66f29
