#ifndef SENSOR_THRESHOLDS_HPP
#define SENSOR_THRESHOLDS_HPP

// ============================================================
//  Jeogi-at Shared Sensor Thresholds
//  Qt 클라이언트와 서버가 공통으로 사용하는 센서 임계값 상수.
//
//  이 파일은 Qt와 서버 양쪽에서 모두 include됩니다.
//  표준 C++ 헤더만 사용하며, 플랫폼 의존성이 없습니다.
// ============================================================

namespace Protocol {

// ──────────────────────────────────────────
//  CO (일산화탄소) 임계값  단위: ppm
// ──────────────────────────────────────────
constexpr double CO_GOOD_MAX = 9.0;     // < 9.0  → 양호
constexpr double CO_CAUTION_MAX = 25.0; // < 25.0 → 주의,  >= 25.0 → 위험

// ──────────────────────────────────────────
//  CO2 (이산화탄소) 임계값  단위: ppm
// ──────────────────────────────────────────
constexpr double CO2_GOOD_MAX = 400.0;    // < 400  → 양호
constexpr double CO2_CAUTION_MAX = 600.0; // < 600  → 주의,  >= 600  → 위험

// ──────────────────────────────────────────
//  혼잡도 임계값  단위: 명(승객 수)
// ──────────────────────────────────────────
constexpr int CONGESTION_EASY_MAX = 1;    // < 0  → 여유
constexpr int CONGESTION_NORMAL_MAX = 2; // < 1 → 보통, >= 2 → 혼잡

// ──────────────────────────────────────────
//  상태 레이블 (Qt UI 표기)
// ──────────────────────────────────────────
constexpr const char *STATUS_GOOD = "🟢 양호";
constexpr const char *STATUS_CAUTION = "🟡 주의";
constexpr const char *STATUS_DANGER = "🔴 위험";

constexpr const char *CONGESTION_EASY = "여유";
constexpr const char *CONGESTION_NORMAL = "보통";
constexpr const char *CONGESTION_BUSY = "혼잡";

} // namespace Protocol

#endif // SENSOR_THRESHOLDS_HPP
