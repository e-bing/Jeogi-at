# 🖥️ 데이터 수집 및 분석 메인 서버

지하철 승강장 구역별 혼잡도 안내 및 환경 관리 시스템(저기어때?)의 메인 서버 모듈입니다.
카메라 엣지 노드(Hanwha/Pi Node)에서 수집된 영상을 디코딩하고 메타데이터(혼잡도)를 병합·분석하며, 센서 데이터를 취합해 장치 제어 노드(모터, 디스플레이, 오디오 등)로 명령을 내리거나 DB에 저장하는 역할을 수행합니다. 또한 관리자 클라이언트(Qt)와 TLS 통신을 통해 관제 환경을 제공합니다.

---

## 🤍 1. 주요 기능

* **이기종 카메라 통합 수신**

  * **Hanwha Node:** RTSP를 통해 실시간 영상과 감지된 객체 객체 메타데이터(XML) 수신 및 파싱

  * **Raspberry Pi Node:** TCP(포트 5000) 기반의 GStreamer H.264 스트림과 MQTT로 발행된 객체 탐지(JSON) 데이터 연동

* **혼잡도 분석 및 장치 제어(MQTT)**

  * 카메라에서 수집된 인원수 데이터를 종합 분석(Congestion Analyzer)하여 8개 구역별 혼잡도(원활, 보통, 혼잡) 산출

  * 산출된 혼잡도 결과를 기반으로 모터, 오디오, 디스플레이 제어 명령을 MQTT로 Publish

* **센서 데이터 및 통계 수집 (DB 저장)**

  * 온/습도, 유해가스, 일산화탄소 센서 데이터를 수집하고 MySQL(MariaDB)에 이력 저장

* **보안 통신 (TLS/SSL)**

  * 관리자용 클라이언트 앱(Qt)과의 데이터 송수신을 위한 안전한 OpenSSL 기반 서버 소켓 환경 제공

---

## 🤍 2. 시스템 환경 및 기술 스택

* **Language:** C++17

* **Framework & Library:**

  * **Multimedia:** FFmpeg, OpenCV
  
  * **Network & Security:** OpenSSL, Eclipse Paho MQTT (C/C++), nlohmann/json

  * **Database:** MariaDB (MySQL C Connector)

  * **XML Parsing:** tinyxml2

* **Build System:** CMake

---

## 🤍 3. 폴더 구조

```text
server/
 ├─ config/           # 서버 설정 파일 및 인증서 보관
 ├─ includes/         # 서버 전체 헤더 파
 ├─ src/              # 서버 코어 로직 소스 코드
 │   ├─ main.cpp               # 메인 실행 파일
 │   ├─ hanwha_node.cpp / pi_node.cpp   # 카메라 스트리밍 및 메타데이터 수신
 │   ├─ congestion_*.cpp       # 혼잡도 분석 및 브로드캐스트 로직
 │   └─ motor/audio/sensor.cpp # 장치 및 센서 제어 통신 로직
 ├─ build.sh          # 빌드 자동화 스크립트
 ├─ setup_server.sh   # 의존성 패키지 및 MariaDB, MQTT(Mosquitto) 설치 스크립트
 ├─ get_cert.sh       # TLS 인증서 생성 스크립트
 └─ CMakeLists.txt    # CMake 빌드 설정
```

---

## 🤍 4. 빌드 및 실행 가이드 (Build & Run)

이 프로젝트를 로컬 환경에서 빌드하고 실행하기 위한 단계별 안내입니다.

### 1. 환경 설정 및 준비 (최초 1회)

서버 실행에 필요한 라이브러리를 설치하고 보안 인증서를 설정합니다.

* **라이브러리 설치:**
```bash
chmod +x setup_server.sh
./setup_server.sh
```

* **보안 인증서 자동 생성:**

SSL/TLS 보안 통신을 위한 인증서를 생성합니다. 실행 시 현재 서버 IP를 자동으로 감지하여 config/ 폴더에 저장합니다.

```bash
chmod +x get_cert.sh
./get_cert.sh
```

> 💡 **참고:** 생성 완료 후, 안내되는 IP 주소를 확인하여 클라이언트에서 서버 IP 값을 해당 IP로 수정해야 정상적으로 동작합니다.

* **시스템 설정 (Unified AI Monitor Setup):**

빌드가 완료되면 아래 명령어를 실행하여 카메라 IP, ID/PW 및 라즈베리 파이 카메라 설정을 진행하세요. 이 과정에서 `config.json` 파일이 자동으로 생성됩니다.

```bash
cd build
chmod +x setup
./setup
```


> 💡 **참고:** 한화 카메라 설정(IP, 계정 정보), 라즈베리 파이 카메라 IP를 미리 준비해 주세요.
> RTSP Profile과 MQTT Topic은 줄바꿈 문자(엔터)를 입력하여 넘겨 주세요.


### 2. 데이터베이스 생성

MariaDB에 접속 후 아래 명령어를 순서대로 실행합니다.

```bash
CREATE DATABASE jeogi;
USE jeogi;
```

```sql
CREATE TABLE station_stats (
    station_id    INT PRIMARY KEY,
    station_name  VARCHAR(50) NOT NULL,
    location_info VARCHAR(100)
);

INSERT INTO station_stats (station_id, station_name, location_info)
VALUES (1, 'Jeogi-Station', 'Seoul, Line 2');
```

```sql
CREATE TABLE camera_stats (
    id              INT AUTO_INCREMENT PRIMARY KEY,
    station_id      INT,
    camera_id       VARCHAR(20) NOT NULL,
    platform_no     VARCHAR(10) NOT NULL,
    passenger_count INT NOT NULL,
    congestion_stat TINYINT,
    status_msg      VARCHAR(50),
    recorded_at     DATETIME NOT NULL,
    INDEX (recorded_at)
);
```

```sql
CREATE TABLE air_stats (
    id              INT AUTO_INCREMENT PRIMARY KEY,
    station_id      INT,
    co_level        FLOAT NOT NULL,
    toxic_gas_level FLOAT NOT NULL,
    temperature     FLOAT NULL,
    humidity        FLOAT NULL,
    fire_detected   TINYINT DEFAULT 0,
    recorded_at     DATETIME NOT NULL,
    INDEX (recorded_at)
);
```

### 3. 프로젝트 빌드 및 설정 파일 생성

제공된 `build.sh` 스크립트를 이용해 소스 코드를 빌드합니다.

* **빌드:**

```bash
./build.sh

# 기존 빌드를 지우고 재빌드하려면
# ./build.sh clean
```


* **설정 파일 생성:**
빌드가 완료되면, `build` 디렉토리로 이동하여 `setup` 프로그램을 실행해 카메라 IP, ID/PW 등 초기 설정을 진행합니다.

```bash
cd build
chmod +x setup
./setup
```

> 💡 **참고:** 이 과정을 거치면 `config.json` 파일이 자동 생성됩니다. 민감한 비밀번호가 포함되어 있으므로 외부에 유출되지 않도록 주의해 주세요.

### 4. 서버 실행

모든 준비가 완료되면 `build` 폴더 안에서 서버를 구동합니다.

```bash
cd build
./server
```

---

### 💡 참고 사항

* 스크립트 실행 권한 에러 발생 시 `chmod +x *.sh`를 입력하여 권한을 부여할 수 있습니다.

* 실행 중인 터미널에서 `Ctrl+C`를 누르면 서버 내의 스레드와 자원들이 안전하게 해제되며 종료됩니다.


