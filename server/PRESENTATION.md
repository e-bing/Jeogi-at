# 지오기앳(Jeogi-at) 서버 기술 발표

> 스마트 지하철 역사 모니터링 시스템 - 서버 파트

---

## 1. 시스템 전체 구조

```
[한화 IP 카메라] ──RTSP──┐
[라즈베리파이 CAM_01] ──MQTT──┤
[라즈베리파이 CAM_02] ──MQTT──┤    [C++ 서버 (Linux)]    ──TLS/SSL──▶ [Qt 클라이언트]
[STM32 펌웨어] ────MQTT──┤         (포트 12345)
   (CO, CO2, 온습도)       │             │
                           │             ▼
                           │          [MySQL DB]
                           │          air_stats
                           │          camera_stats
                           └──────────────┘
                              MQTT Broker
                            (localhost:1883)
```

### 데이터 흐름 요약

| 데이터 | 출처 | 전달 경로 | 목적지 |
|--------|------|-----------|--------|
| 영상 스트림 | 한화 카메라 / Pi | RTSP → 서버 → JPEG | Qt (실시간 영상) |
| 혼잡도 분석 | CongestionAnalyzer | 메모리 → 서버 | Qt, STM32, 드라이버 |
| CO / CO2 | STM32 | MQTT → 서버 → DB | Qt 실시간/통계 |
| 온습도 | STM32 | MQTT → 서버 → DB | Qt 통계 |
| 시스템 모니터 | /proc, /sys, MQTT | 서버 수집 → TLS | Qt 대시보드 |

---

## 2. 핵심 모듈별 기술 설명

### 2-1. CongestionAnalyzer — ROI 기반 혼잡도 분석

> **포인트**: 픽셀 좌표 대신 정규화된 비율 좌표(0.0~1.0)로 ROI를 정의해 해상도에 독립적으로 동작

```cpp
// config.json에서 구역 설정 로드 (정규화 좌표)
"zones": [
  { "zone_id": 1, "camera_id": "hanwha", "roi": [0.0, 0.0, 0.25, 1.0] },
  { "zone_id": 2, "camera_id": "hanwha", "roi": [0.25, 0.0, 0.5,  1.0] },
  ...
]
```

- 한화 카메라(4구역) + Pi CAM_01(2구역) + Pi CAM_02(2구역) = **총 8구역 독립 분석**
- 객체의 중심점(centerX, centerY)이 ROI 내부에 있는지 판별 (`isInside()`)
- 구역별 인원 수 → `calculateLevel()`로 혼잡도 레벨(0/1/2) 변환

```
0: EASY    (임계값 이하)
1: NORMAL  (중간)
2: CROWDED (초과)
```

- 분석 루프는 **별도 스레드**에서 실행 → `std::mutex`로 results 보호
- `getCongestionCounts()` — 구역별 실제 인원 수
- `getCongestionLevels()` — 구역별 혼잡도 레벨
- `getCameraIds()` — 구역별 카메라 ID (DB 저장 시 camera_id 매핑에 사용)

---

### 2-2. client_handler — TLS 멀티스레드 Qt 연결 처리

> **포인트**: 단일 Qt 클라이언트 연결에 4개 스레드를 분리해 영상/데이터/명령을 동시 처리

```
handle_client()
  ├── reader_thread   : Qt → 서버 명령 수신 (모터, 디스플레이, 오디오 등)
  ├── writer_thread   : 서버 → Qt 데이터 송신 (패킷 큐 기반)
  ├── hanwha_worker   : 한화 카메라 YUV→JPEG 변환 + 전송 (100fps 목표)
  └── pi_worker       : Pi 카메라 프레임 전송 (33fps 목표)
```

**주기별 송신 데이터:**

| 주기 | 데이터 | 방식 |
|------|--------|------|
| 100ms | 구역별 혼잡도 레벨/인원수 | 메모리에서 직접 |
| 1초 | 실시간 공기질, 흐름 통계 | DB 조회 |
| 5초 | 온습도 통계, 승객 흐름 통계, 시스템 모니터 | DB 조회 |

- 패킷 큐(`queue<vector<uint8_t>>`) + `condition_variable`로 writer 스레드 효율화
- 영상은 길이 prefix 프로토콜: `[4byte 길이][JPEG 데이터]`
- TLS 1.2/1.3 지원 (OpenSSL)

---

### 2-3. sensor.cpp — 멀티 토픽 MQTT 수신 및 캐싱

> **포인트**: CO/CO2와 온습도가 **다른 토픽**으로 들어오므로, 온습도를 캐시해 두고 CO/CO2 수신 시 통합 저장

```
sensor/air_quality  →  CO, CO2 수신
                        + 캐시된 온습도 합쳐서 → DB 저장 (save_sensor_data)

sensor/temp_humi    →  온도, 습도만 캐시 업데이트
```

- 연결 끊김 시 **5초 후 자동 재연결** 루프
- 수신 즉시 `save_sensor_data()` 호출 → `air_stats` 테이블에 INSERT

---

### 2-4. system_monitor.cpp — 서버 + 펌웨어 이중 모니터링

> **포인트**: 서버 자체 리소스는 `/proc/stat`, `/sys/class/thermal`에서 직접 읽고, STM32 펌웨어 상태는 MQTT로 수신해 합산 후 Qt로 전송

```
[서버 자체]                  [STM32 펌웨어]
/proc/stat → CPU 사용률      MQTT system/firmware
/sys/class/thermal → 온도    → CPU, Temp, Disk 수신
/proc/statvfs → 디스크
        │                           │
        └──────────── 합산 JSON ────┘
                          │
                      TLS → Qt
```

- CPU 사용률: idle/total 비율로 직접 계산 (0.5초 샘플링)
- 서버 + 펌웨어 두 소스의 모니터링 데이터를 단일 `system_monitor` 메시지로 통합

---

### 2-5. database.cpp — 시뮬레이션 타임 가속 + 통계 집계

> **포인트**: 테스트를 위해 DB 저장 시 타임스탬프를 3600배 가속 — 실제 1초 경과가 DB에는 1시간으로 기록

```cpp
string get_sim_timestamp() {
  // real_start + 경과 실제 초 × scale → 시뮬 시각
  // scale = 3600: 1초 = 1시간
}
```

- 3분 실행으로 일주일치 통계 데이터 생성 가능
- `DAYOFWEEK()`, `HOUR()`로 GROUP BY → 요일×시간 히트맵 통계
- 카메라 3개(hanwha/CAM_01/CAM_02)별로 platform_no(1~8) 분리 저장

**통계 쿼리 구조:**

```sql
SELECT DAYOFWEEK(recorded_at), HOUR(recorded_at), platform_no,
       AVG(passenger_count), AVG(congestion_stat)
FROM camera_stats
GROUP BY DAYOFWEEK, HOUR, platform_no;
```

---

### 2-6. motor.cpp — MQTT 기반 모터 제어

- Qt 명령 수신 → `send_motor_command(action, speed)` → MQTT 발행
- auto/manual 모드 전환: `send_mode_command()`
- JSON 페이로드로 STM32에 전달

---

## 3. 트러블슈팅

### 3-1. MQTT 이중 발행 버그

**증상**: STM32 드라이버에 동일 명령이 두 번 전달됨

**원인**: `send_display_command()`에서 `MQTT_TOPIC`과 `MQTT_TOPIC_CONGESTION_STATUS` 두 토픽에 동시 발행

```cpp
// 문제 코드
g_mqtt_client->publish(MQTT_TOPIC, payload, 1, false)->wait();
g_mqtt_client->publish(Protocol::MQTT_TOPIC_CONGESTION_STATUS, payload, 1, false)->wait(); // ← 제거
```

**해결**: 두 번째 발행 라인 제거

---

### 3-2. camera_stats 통계 빈 시간대 발생

**증상**: 요일별 통계에서 특정 시간(예: 1시, 12시)이 누락

**원인**: DB 저장 주기 5초 × time_scale 3600 = 5시간 간격으로 INSERT → 건너뛰는 시간 발생

```
저장 주기 5초 × 3600배 = 5시간씩 건너뜀
```

**해결**: DB 저장 tick 500 → 100으로 변경 (5초 → 1초 주기)

```cpp
if (db_tick >= 100)  // 500 → 100
```

---

### 3-3. get_temp_humi_stats JOIN 실패 → NULL 반환

**증상**: Qt 온습도 통계가 전부 NULL

**원인**: `camera_stats JOIN air_stats ON recorded_at = recorded_at` — 두 테이블의 INSERT 타이밍이 달라 정확히 일치하는 타임스탬프가 없어 JOIN 결과 없음

**해결**: JOIN 제거, `air_stats`만 단독 조회로 변경

```sql
-- 수정 전 (JOIN 실패)
FROM camera_stats C JOIN air_stats A ON C.recorded_at = A.recorded_at ...

-- 수정 후
FROM air_stats WHERE station_id = 1 AND temperature IS NOT NULL
```

---

### 3-4. 하드코딩된 "CAM-01"로 카메라 3개 데이터가 단일 ID로 저장

**증상**: `camera_stats`의 `camera_id`가 모두 `"CAM-01"` → 구역별 카메라 구분 불가

**원인**: `save_camera_stats()` 내부에 `camera_id`가 `'CAM-01'`로 하드코딩

**해결**: `CongestionAnalyzer::getCameraIds()`를 추가해 zones 설정에서 구역별 카메라 ID를 가져와 INSERT

```cpp
save_camera_stats(conn, g_analyzer.getCongestionCounts(),
                        g_analyzer.getCongestionLevels(),
                        g_analyzer.getCameraIds());  // ← 추가
```

---

### 3-5. zones 미설정으로 passenger_count 전부 0 저장

**증상**: DB에 `passenger_count=0`, `congestion_stat=0`만 쌓임

**원인**: `config.json`에 `zones` 필드 누락 → CongestionAnalyzer가 ROI 없이 항상 0 반환

**해결**: `config.json`에 8개 구역 ROI 설정 추가 (정규화 좌표)

---

## 4. 기술적 하이라이트

| 항목 | 내용 |
|------|------|
| 통신 보안 | Qt ↔ 서버 TLS 1.2/1.3 (OpenSSL) |
| 멀티스레드 | 클라이언트당 4스레드 (reader/writer/hanwha/pi) |
| 카메라 통합 | 3종 카메라(한화 RTSP + Pi MJPEG × 2) 동시 스트리밍 |
| ROI 분석 | 정규화 좌표 기반 8구역 독립 혼잡도 분석 |
| 시간 가속 | 3600배 타임스탬프로 테스트 시 3분 = 1주일치 통계 생성 |
| MQTT 멀티 브로커 | 모터/스피커/디스플레이/센서 각각 독립 MQTT 연결 |
| 프로토콜 중앙화 | `message_types.hpp`에 모든 토픽/필드명 상수 관리 |
