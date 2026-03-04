
## 🚀 빌드 및 실행 가이드 (Build & Run)

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
⚠️ 주의: 생성 완료 후, 안내되는 IP 주소를 확인하여 클라이언트에서 서버 IP 값을 해당 IP로 수정해야 정상적으로 동작합니다.

* **시스템 설정 (Unified AI Monitor Setup):**
빌드가 완료되면 아래 명령어를 실행하여 카메라 IP, ID/PW 및 라즈베리 파이 카메라 설정을 진행하세요. 이 과정에서 `config.json` 파일이 자동으로 생성됩니다.
```bash
cd build
chmod +x setup
./setup
```


> **참고:** 한화 카메라 설정(IP, 계정 정보), 라즈베리 파이 카메라 IP를 미리 준비해 주세요.
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



### 3. 프로젝트 빌드

스크립트를 이용해 소스 코드를 컴파일합니다.

* **일반 빌드:**
```bash
./build.sh
```


* **빌드 폴더 초기화 후 빌드 (Clean Build):**
기존 빌드 내역을 삭제하고 깨끗한 상태에서 다시 빌드합니다.
```bash
./build.sh clean
```



### 4. 서버 실행

빌드가 완료되면 `build` 폴더로 이동하여 서버를 실행합니다.

```bash
cd build
./server
```

---

### 💡 참고 사항

* 스크립트 실행 권한 에러 발생 시 `chmod +x *.sh`를 입력하여 권한을 부여할 수 있습니다.

* **설정 파일:** `./setup`을 통해 생성된 `config.json`에는 민감한 비밀번호가 포함되어 있으므로 외부에 유출되지 않도록 주의해 주세요.
