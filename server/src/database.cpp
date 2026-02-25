// database.cpp
#include "database.h"

#include <iostream>

MYSQL* connect_db(DBConfig config) {
  MYSQL* conn = mysql_init(NULL);
  if (!mysql_real_connect(conn, config.host.c_str(), config.user.c_str(),
                          config.pass.c_str(), config.db.c_str(), 0, NULL, 0)) {
    cerr << "DB 연결 실패: " << mysql_error(conn) << endl;
    return nullptr;
  }
  cout << "✅ DB 연결 성공: " << config.db << endl;
  return conn;
}

void close_db(MYSQL* conn) {
  if (conn) {
    mysql_close(conn);
    cout << "DB 연결 종료" << endl;
  }
}

bool save_sensor_data(MYSQL* conn, float co_value, float co2_value) {
  string sql =
      "INSERT INTO air_stats (station_id, co_level, toxic_gas_level, "
      "fire_detected, recorded_at) "
      "VALUES (1, " +
      to_string(co_value) + ", " + to_string(co2_value) + ", 0, NOW())";

  if (mysql_query(conn, sql.c_str())) {
    cerr << "❌ DB 저장 실패: " << mysql_error(conn) << endl;
    return false;
  }

  cout << "✅ DB 저장 완료: CO = " << co_value << " ppm, CO2 = " << co2_value
       << " ppm" << endl;
  return true;
}

json get_realtime_congestion(MYSQL* conn) {
  string sql = R"(
        SELECT
            S.station_name,
            C.platform_no,
            C.passenger_count,
            C.status_msg
        FROM camera_stats C
        JOIN station_stats S ON C.station_id = S.station_id
        INNER JOIN (
            SELECT platform_no, MAX(recorded_at) as max_time
            FROM camera_stats
            GROUP BY platform_no
        ) Latest ON C.platform_no = Latest.platform_no AND C.recorded_at = Latest.max_time
        ORDER BY
            CAST(SUBSTRING_INDEX(C.platform_no, '-', 1) AS UNSIGNED) ASC,
            CAST(SUBSTRING_INDEX(C.platform_no, '-', -1) AS UNSIGNED) ASC;
    )";

  json result = json::array();

  if (mysql_query(conn, sql.c_str())) {
    cerr << "실시간 혼잡도 쿼리 실패: " << mysql_error(conn) << endl;
    return result;
  }

  MYSQL_RES* res = mysql_store_result(conn);
  MYSQL_ROW row;

  while ((row = mysql_fetch_row(res))) {
    json item = {{"station", row[0] ? row[0] : "N/A"},
                 {"platform", row[1] ? row[1] : "N/A"},
                 {"count", row[2] ? stoi(row[2]) : 0},
                 {"status", row[3] ? row[3] : "정상"}};
    result.push_back(item);
  }

  mysql_free_result(res);
  return result;
}

json get_realtime_air_quality(MYSQL* conn) {
  string sql = R"(
        SELECT
            station_id,
            co_level,
            toxic_gas_level,
            fire_detected,
            recorded_at
        FROM air_stats
        WHERE station_id = 1
        ORDER BY recorded_at DESC
        LIMIT 1;
    )";

  json result = json::array();

  if (mysql_query(conn, sql.c_str())) {
    cerr << "실시간 공기질 쿼리 실패: " << mysql_error(conn) << endl;
    return result;
  }

  MYSQL_RES* res = mysql_store_result(conn);
  MYSQL_ROW row;

  if ((row = mysql_fetch_row(res))) {
    json item = {{"station", "Jeogi-Station"},
                 {"co_level", row[1] ? stod(row[1]) : 0.0},
                 {"co2_ppm", row[2] ? stod(row[2]) : 0.0},
                 {"fire_detected", row[3] ? stoi(row[3]) == 1 : false},
                 {"recorded_at", row[4] ? row[4] : "N/A"}};
    result.push_back(item);
  }

  mysql_free_result(res);
  return result;
}

json get_air_quality_stats(MYSQL* conn, string cam_id) {
  string sql = R"(
        SELECT
            DAYOFWEEK(C.recorded_at) AS d_idx,
            HOUR(C.recorded_at) AS hour,
            AVG(A.co_level) AS avg_co,
            AVG(A.toxic_gas_level) AS avg_co2
        FROM camera_stats C
        JOIN air_stats A ON C.recorded_at = A.recorded_at AND C.station_id = A.station_id
        WHERE C.camera_id = ')" +
               cam_id + R"('
        GROUP BY d_idx, hour;
    )";

  json result = json::array();

  if (mysql_query(conn, sql.c_str())) {
    cerr << "공기질 쿼리 실패: " << mysql_error(conn) << endl;
    return result;
  }

  MYSQL_RES* res = mysql_store_result(conn);
  MYSQL_ROW row;

  while ((row = mysql_fetch_row(res))) {
    json item = {{"day", row[0] ? stoi(row[0]) : 0},
                 {"hour", row[1] ? stoi(row[1]) : 0},
                 {"co", row[2] ? stod(row[2]) : 0.0},
                 {"co2", row[3] ? stod(row[3]) : 0.0}};
    result.push_back(item);
  }

  mysql_free_result(res);
  return result;
}

json get_passenger_flow_stats(MYSQL* conn, string cam_id) {
  string sql = R"(
        SELECT
            DAYOFWEEK(recorded_at) AS d_idx,
            HOUR(recorded_at) AS hour,
            AVG(passenger_count) AS avg_p
        FROM camera_stats
        WHERE camera_id = ')" +
               cam_id + R"('
        GROUP BY d_idx, hour;
    )";

  json result = json::array();

  if (mysql_query(conn, sql.c_str())) {
    cerr << "승객 흐름 쿼리 실패: " << mysql_error(conn) << endl;
    return result;
  }

  MYSQL_RES* res = mysql_store_result(conn);
  MYSQL_ROW row;

  while ((row = mysql_fetch_row(res))) {
    json item = {{"day", row[0] ? stoi(row[0]) : 0},
                 {"hour", row[1] ? stoi(row[1]) : 0},
                 {"avg_count", row[2] ? (int)stod(row[2]) : 0}};
    result.push_back(item);
  }

  mysql_free_result(res);
  return result;
}
