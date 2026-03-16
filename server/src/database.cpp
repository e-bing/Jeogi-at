// database.cpp
#include "database.hpp"

#include <chrono>
#include <ctime>
#include <iostream>
#include <vector>

#include "../../protocol/message_types.hpp"
#include "config_manager.hpp"
#include "shared_data.hpp"

static std::string get_sim_timestamp() {
  static auto real_start = std::chrono::system_clock::now();

  int scale = 3600;

  auto now = std::chrono::system_clock::now();
  long long elapsed =
      std::chrono::duration_cast<std::chrono::seconds>(now - real_start)
          .count();

  auto sim = real_start + std::chrono::seconds(elapsed * scale);
  std::time_t t = std::chrono::system_clock::to_time_t(sim);
  std::tm* tm = std::localtime(&t);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
  return std::string(buf);
}

/**
 * @brief config.json에서 DB 접속 정보를 읽어 연결합니다.
 *        setup.cpp에서 최초 1회 설정 후 사용합니다.
 */
MYSQL* connect_db() {
  auto config = ConfigManager::load();

  string host = config["db"].value("host", "localhost");
  string user = config["db"].value("user", "");
  string pass = config["db"].value("pass", "");
  string db = config["db"].value("name", "jeogi");

  MYSQL* conn = mysql_init(NULL);
  if (!mysql_real_connect(conn, host.c_str(), user.c_str(), pass.c_str(),
                          db.c_str(), 0, NULL, 0)) {
    cerr << "❌ DB 연결 실패: " << mysql_error(conn) << endl;
    return nullptr;
  }
  std::cout << "✅ DB 연결 성공: " << db << endl;
  return conn;
}

void close_db(MYSQL* conn) {
  if (conn) {
    mysql_close(conn);
    std::cout << "DB 연결 종료" << endl;
  }
}

bool save_sensor_data(MYSQL* conn, float co, float co2, float temp,
                      float humi) {
  string sql =
      "INSERT INTO air_stats (station_id, co_level, toxic_gas_level, "
      "temperature, humidity, fire_detected, recorded_at) "
      "VALUES (1, " +
      to_string(co) + ", " + to_string(co2) + ", " + to_string(temp) + ", " +
      to_string(humi) + ", 0, '" + get_sim_timestamp() + "')";

  if (mysql_query(conn, sql.c_str())) {
    cerr << "❌ DB 통합 저장 실패: " << mysql_error(conn) << endl;
    return false;
  }
  std::cout << "✅ DB 통합 저장 완료 (CO/CO2/Temp/Humi)" << endl;
  return true;
}

bool save_camera_stats(MYSQL* conn, const std::vector<int>& counts,
                       const std::vector<int>& levels,
                       const std::vector<std::string>& cam_ids) {
  for (int i = 0; i < 8; ++i) {
    const std::string& cam_id = cam_ids[i];

    // 해당 카메라 연결 여부 확인
    bool connected = false;
    if (cam_id == "hanwha") {
      std::lock_guard<std::mutex> lock(g_hw_frame_mutex);
      connected = !g_hw_frame_buffer.empty();
    } else {
      std::lock_guard<std::mutex> lock(g_node_map_mutex);
      auto it = g_pi_node_map.find(cam_id);
      if (it != g_pi_node_map.end()) {
        std::lock_guard<std::mutex> cam_lock(it->second->data_mutex);
        connected = !it->second->frame_buffer.empty();
      }
    }
    if (!connected) continue;

    string sql =
        "INSERT INTO camera_stats (station_id, camera_id, platform_no, "
        "passenger_count, congestion_stat, recorded_at) "
        "VALUES (1, '" +
        cam_id + "', '" + to_string(i + 1) + "', " + to_string(counts[i]) +
        ", " + to_string(levels[i]) + ", '" + get_sim_timestamp() + "')";

    if (mysql_query(conn, sql.c_str())) {
      cerr << "❌ camera_stats 저장 실패 (platform " << (i + 1)
           << "): " << mysql_error(conn) << endl;
      return false;
    }
  }
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
  if (!res) return result;
  MYSQL_ROW row;

  while ((row = mysql_fetch_row(res))) {
    json item = {{Protocol::FIELD_STATION, row[0] ? row[0] : "N/A"},
                 {Protocol::FIELD_PLATFORM, row[1] ? row[1] : "N/A"},
                 {Protocol::FIELD_COUNT, row[2] ? stoi(row[2]) : 0},
                 {Protocol::FIELD_STATUS, row[3] ? row[3] : "정상"}};
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
            temperature,
            humidity,
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
  if (!res) return result;
  MYSQL_ROW row;

  if ((row = mysql_fetch_row(res))) {
    json item = {
        {Protocol::FIELD_STATION, "Jeogi-Station"},
        {Protocol::FIELD_CO_LEVEL, row[1] ? stod(row[1]) : 0.0},
        {Protocol::FIELD_CO2_PPM, row[2] ? stod(row[2]) : 0.0},
        {Protocol::FIELD_TEMP, row[3] ? stod(row[3]) : 0.0},
        {Protocol::FIELD_HUMI, row[4] ? stod(row[4]) : 0.0},
        {Protocol::FIELD_FIRE_DETECTED, row[5] ? stoi(row[5]) == 1 : false},
        {Protocol::FIELD_RECORDED_AT, row[6] ? row[6] : "N/A"}};
    result.push_back(item);
  }

  mysql_free_result(res);
  return result;
}

json get_air_quality_stats(MYSQL* conn) {
  string sql = R"(
        SELECT
            DAYOFWEEK(recorded_at) AS d_idx,
            HOUR(recorded_at) AS hour,
            AVG(co_level) AS avg_co,
            AVG(toxic_gas_level) AS avg_co2
        FROM air_stats
        WHERE station_id = 1
        GROUP BY d_idx, hour;
    )";

  json result = json::array();

  if (mysql_query(conn, sql.c_str())) {
    cerr << "공기질 쿼리 실패: " << mysql_error(conn) << endl;
    return result;
  }

  MYSQL_RES* res = mysql_store_result(conn);
  if (!res) return result;
  MYSQL_ROW row;

  while ((row = mysql_fetch_row(res))) {
    json item = {{Protocol::FIELD_DAY, row[0] ? stoi(row[0]) : 0},
                 {Protocol::FIELD_HOUR, row[1] ? stoi(row[1]) : 0},
                 {Protocol::FIELD_CO, row[2] ? stod(row[2]) : 0.0},
                 {Protocol::FIELD_CO2, row[3] ? stod(row[3]) : 0.0}};
    result.push_back(item);
  }

  mysql_free_result(res);
  return result;
}

json get_temp_humi_stats(MYSQL* conn) {
  string sql = R"(
        SELECT
            DAYOFWEEK(recorded_at) AS d_idx,
            HOUR(recorded_at) AS hour,
            AVG(temperature) AS avg_temp,
            AVG(humidity) AS avg_humi
        FROM air_stats
        WHERE station_id = 1
          AND temperature IS NOT NULL
          AND humidity IS NOT NULL
        GROUP BY d_idx, hour;
    )";

  json result = json::array();

  if (mysql_query(conn, sql.c_str())) {
    cerr << "온습도 통계 쿼리 실패: " << mysql_error(conn) << endl;
    return result;
  }

  MYSQL_RES* res = mysql_store_result(conn);
  if (!res) return result;
  MYSQL_ROW row;

  while ((row = mysql_fetch_row(res))) {
    json item = {{Protocol::FIELD_DAY, row[0] ? stoi(row[0]) : 0},
                 {Protocol::FIELD_HOUR, row[1] ? stoi(row[1]) : 0},
                 {Protocol::FIELD_TEMP, row[2] ? stod(row[2]) : 0.0},
                 {Protocol::FIELD_HUMI, row[3] ? stod(row[3]) : 0.0}};
    result.push_back(item);
  }

  mysql_free_result(res);
  return result;
}

json get_passenger_flow_stats(MYSQL* conn) {
  string sql = R"(
        SELECT
            DAYOFWEEK(recorded_at) AS d_idx,
            HOUR(recorded_at) AS hour,
            platform_no,
            AVG(passenger_count) AS avg_p,
            AVG(congestion_stat) AS avg_c
        FROM camera_stats
        WHERE station_id = 1
        GROUP BY d_idx, hour, platform_no
        ORDER BY d_idx, hour, CAST(platform_no AS UNSIGNED);
    )";

  json result = json::array();

  if (mysql_query(conn, sql.c_str())) {
    cerr << "승객 흐름 쿼리 실패: " << mysql_error(conn) << endl;
    return result;
  }

  MYSQL_RES* res = mysql_store_result(conn);
  if (!res) return result;
  MYSQL_ROW row;

  while ((row = mysql_fetch_row(res))) {
    json item = {{Protocol::FIELD_DAY, row[0] ? stoi(row[0]) : 0},
                 {Protocol::FIELD_HOUR, row[1] ? stoi(row[1]) : 0},
                 {"platform", row[2] ? stoi(row[2]) : 0},
                 {Protocol::FIELD_AVG_COUNT, row[3] ? (int)stod(row[3]) : 0},
                 {"avg_congestion", row[4] ? stod(row[4]) : 0.0}};
    result.push_back(item);
  }

  mysql_free_result(res);
  return result;
}
