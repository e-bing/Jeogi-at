# 🚀 빌드 및 실행 가이드 (Build & Run)

이 프로젝트를 라즈베리파이 환경에서 빌드하고 실행하기 위한 단계별 안내입니다.

---

### 1. 환경 설정 및 준비 (최초 1회)

실행에 필요한 라이브러리와 드라이버 빌드 환경을 설치합니다.

```bash
chmod +x setup_drivers.sh
./setup_drivers.sh
```

설치 항목:
- `build-essential`, `linux-headers`, `libi2c-dev`, `nlohmann-json3-dev`
- Paho MQTT C / C++ 라이브러리 (소스 빌드)
- I2C 활성화 여부 확인

> ⚠️ **참고:** I2C가 비활성화된 경우 `sudo raspi-config → Interface Options → I2C → Enable` 후 재부팅이 필요합니다.

---

### 2. 설정 파일 수정

`config/config.json`에 MQTT 브로커 주소와 토픽을 설정합니다.

```json
{
    "mqtt_broker": "tcp://<서버 IP>:1883",
    "sensor_topic": "sensor/air_quality",
    "sht20_topic": "sensor/temp_humi"
}
```

> **참고:** `mqtt_broker`의 IP는 서버 측 IP로 변경해 주세요.

---

### 3. 빌드 및 드라이버 로드

소스 코드를 컴파일하고 커널 모듈을 로드합니다.

```bash
chmod +x build.sh
./build.sh
```

수행 순서:
1. `make all` — 커널 모듈(`.ko`) + 유저 앱(`drivers`) 빌드
2. `insmod motor_driver.ko` — 모터 PWM/GPIO 커널 드라이버 로드
3. `insmod sht20_driver.ko` — SHT20 온습도 I2C 커널 드라이버 로드
4. `/dev/motor`, `/dev/sht20` 권한 설정

> ⚠️ **주의:** `build.sh`는 `sudo` 권한이 필요합니다. 커널 모듈이 이미 로드된 경우 `⚠️ 이미 로드됨 (정상)` 메시지가 출력되며 정상 동작입니다.

---

### 4. 실행

빌드가 완료되면 드라이버 애플리케이션을 실행합니다.

```bash
./build/drivers
```

---

### 💡 참고 사항

- 스크립트 실행 권한 오류 발생 시: `chmod +x *.sh`
- 커널 모듈 로드 상태 확인: `lsmod | grep -E "motor_driver|sht20"`
- 디바이스 파일 확인: `ls /dev/motor /dev/sht20`
- 커널 모듈 수동 제거: `sudo rmmod motor_driver sht20_driver`
- **설정 파일** `config/config.json`에는 네트워크 정보가 포함되므로 외부에 유출되지 않도록 주의해 주세요.
