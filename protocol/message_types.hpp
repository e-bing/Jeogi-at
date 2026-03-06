#ifndef MESSAGE_TYPES_HPP
#define MESSAGE_TYPES_HPP

// ============================================================
//  Jeogi-at Shared Message Protocol
//  Qt 클라이언트 ↔ 서버 간 JSON 통신에서 공통으로 사용하는
//  타입 문자열 및 필드명 상수 모음.
//
//  이 파일은 Qt와 서버 양쪽에서 모두 include됩니다.
//  표준 C++ 헤더만 사용하며, 플랫폼 의존성이 없습니다.
// ============================================================

namespace Protocol {

// ──────────────────────────────────────────
//  Server → Qt  메시지 타입
// ──────────────────────────────────────────

/// 실시간 공기질 (CO, CO2 최신 1건)
constexpr const char *MSG_REALTIME_AIR = "realtime_air";

/// 공기질 통계 (요일·시간대별 평균)
constexpr const char *MSG_AIR_STATS = "air_stats";

/// 승객 흐름 통계 (요일·시간대별 평균 승객 수)
constexpr const char *MSG_FLOW_STATS = "flow_stats";

/// 입구별 혼잡도
constexpr const char *MSG_ZONE_CONGESTION = "zone_congestion";

/// 서버 시스템 모니터 (CPU/메모리 등)
constexpr const char *MSG_SYSTEM_MONITOR = "system_monitor";

/// 온습도 데이터
constexpr const char *MSG_TEMP_HUMI = "temp_humi";

// ──────────────────────────────────────────
//  Qt → Server  메시지 타입
// ──────────────────────────────────────────

/// 장치 제어 명령 (모터, 스피커, 조명 등)
constexpr const char *MSG_DEVICE_COMMAND = "device_command";

// ──────────────────────────────────────────
//  공통 최상위 필드명
// ──────────────────────────────────────────
constexpr const char *FIELD_TYPE = "type";
constexpr const char *FIELD_DATA = "data";
constexpr const char *FIELD_TITLE = "title";
constexpr const char *FIELD_CAMERA = "camera";

// ──────────────────────────────────────────
//  realtime  필드명
// ──────────────────────────────────────────
constexpr const char *FIELD_STATION = "station";
constexpr const char *FIELD_PLATFORM = "platform";
constexpr const char *FIELD_COUNT = "count";
constexpr const char *FIELD_STATUS = "status";

// ──────────────────────────────────────────
//  realtime_air  필드명
// ──────────────────────────────────────────
constexpr const char *FIELD_CO_LEVEL = "co_level";
constexpr const char *FIELD_CO2_PPM = "co2_ppm";
constexpr const char *FIELD_TEMP = "temp";
constexpr const char *FIELD_HUMI = "humi";
constexpr const char *FIELD_FIRE_DETECTED = "fire_detected";
constexpr const char *FIELD_RECORDED_AT = "recorded_at";
constexpr const char *FIELD_CO_STATUS = "co_status";
constexpr const char *FIELD_GAS_STATUS = "gas_status";

// ──────────────────────────────────────────
//  air_stats / flow_stats  필드명
// ──────────────────────────────────────────
constexpr const char *FIELD_DAY = "day";
constexpr const char *FIELD_HOUR = "hour";
constexpr const char *FIELD_CO = "co";
constexpr const char *FIELD_CO2 = "co2";
constexpr const char *FIELD_GAS = "gas"; // Qt 하위호환용 (co2 별칭)
constexpr const char *FIELD_AVG_COUNT = "avg_count";

// ──────────────────────────────────────────
//  zone_congestion  필드명
// ──────────────────────────────────────────
constexpr const char *FIELD_ZONES = "zones";
constexpr const char *FIELD_TOTAL_COUNT = "total_count";

// ──────────────────────────────────────────
//  temp_humi  필드명
// ──────────────────────────────────────────
constexpr const char *FIELD_TEMPERATURE = "temperature";
constexpr const char *FIELD_HUMIDITY = "humidity";

// ──────────────────────────────────────────
//  device_command  필드명 및 장치·액션 값
// ──────────────────────────────────────────
constexpr const char *FIELD_DEVICE = "device";
constexpr const char *FIELD_ACTION = "action";
constexpr const char *FIELD_SPEED = "speed";

// 장치 이름
constexpr const char *DEVICE_MOTOR = "motor";
constexpr const char *DEVICE_SPEAKER = "speaker";
constexpr const char *DEVICE_LIGHTING = "lighting";
constexpr const char *DEVICE_DIGITAL_DISPLAY = "digital_display";
constexpr const char *DEVICE_MODE_CONTROL = "mode_control";

// 액션 값
constexpr const char *ACTION_START = "start";
constexpr const char *ACTION_STOP = "stop";
constexpr const char *ACTION_ON = "on";
constexpr const char *ACTION_OFF = "off";
constexpr const char *ACTION_AUTO = "auto";
constexpr const char *ACTION_MANUAL = "manual";

// ──────────────────────────────────────────
//  MQTT 토픽 (서버 ↔ 펌웨어 간, 참조용)
// ──────────────────────────────────────────
constexpr const char *MQTT_TOPIC_AIR_QUALITY = "sensor/air_quality";
constexpr const char *MQTT_TOPIC_TEMP_HUMI = "sensor/temp_humi";
constexpr const char *MQTT_TOPIC_MOTOR_CONTROL = "motor/control";

// ──────────────────────────────────────────
//  내부 전송용 명령 타입 (Server → Firmware)
// ──────────────────────────────────────────
constexpr const char *MSG_MOTOR_CONTROL = "motor_control";
constexpr const char *MSG_MODE_CONTROL = "mode_control";

} // namespace Protocol

#endif // MESSAGE_TYPES_HPP
