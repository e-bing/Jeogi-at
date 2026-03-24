# 🔧 라즈베리파이 드라이버

**지하철 승강장 구역별 혼잡도 안내 및 환경 관리 시스템 (저기어때?)** 의 드라이버 모듈입니다.
라즈베리파이 환경에서 환경 센서를 수집하고 하드웨어 장치를 제어하며, MQTT를 통해 중앙 서버와 데이터를 송수신합니다.

## 1. Environment setup

## 🤍 1. 주요 기능

* **공기질 센서 모니터링 (CO/CO2)**
  * STM32와 UART 프로토콜로 통신하여 대기질 센서 데이터를 1.2초 주기로 수집하고 MQTT로 발행합니다.

* **온습도 측정 (SHT20)**
  * I2C 커널 드라이버(`sht20_driver.ko`)를 통해 SHT20 센서에서 온도/습도를 2초 주기로 읽어 발행합니다.

* **환기 모터 제어**
  * PWM 커널 드라이버(`motor_driver.ko`)를 통해 환기 모터 속도를 제어합니다.
  * 자동 모드: MQ7, MQ135 센서 각각의 임계값에 따라 속도를 자동 조절 (예: MQ135(대기질 센서) > 600ppm → 100%, MQ135(대기질 센서) > 400ppm → 60%)
  * 수동 모드: MQTT 명령으로 직접 제어

* **오디오/디스플레이 제어**
  * STM32에 WAV 파일 재생 명령 및 LED 디스플레이 제어 명령을 전달합니다.

* **서울 지하철 실시간 도착 정보 연동**
  * 서울 열린데이터광장 Open API를 통해 3호선 실시간 열차 도착 정보를 수신하고 STM32에 전달합니다.

* **MQTT 기반 서버 통신**
  * 센서 데이터(대기질, 온습도, 시스템 상태)를 서버에 발행하고, 모터/오디오/디스플레이 제어 명령을 구독합니다.

* **시스템 상태 모니터링**
  * CPU 사용률, CPU 온도, 디스크 사용량을 5초 주기로 수집하여 발행합니다.

---

## 🤍 2. 시스템 환경 및 기술 스택

* **Hardware:** Raspberry Pi 4

* **Language:** C++17, C (커널 모듈)

* **Framework & Library:**
  * **통신:** Eclipse Paho MQTT C/C++ (`libpaho-mqttpp3`, `libpaho-mqtt3as`)
  * **JSON:** nlohmann/json, cJSON
  * **HTTP:** libcurl (지하철 API 호출)
  * **I2C:** libi2c-dev
  * **동시성:** pthread

* **Kernel Modules:**
  * `motor_driver.ko` — 모터 PWM/GPIO 제어 (`/dev/motor`)
  * `sht20_driver.ko` — SHT20 I2C 드라이버 (`/dev/sht20`)

* **Build System:** Make + Kbuild (커널 모듈)

---

## 🤍 3. 폴더 구조

```text
drivers/
 ├─ src/              # 메인 애플리케이션 소스 및 커널 모듈
 │   ├─ main.cpp          # 진입점, 스레드 초기화
 │   ├─ communication.cpp # UART 프로토콜 + MQTT 클라이언트
 │   ├─ sensor.cpp        # 대기질 센서 폴링
 │   ├─ motor.cpp         # 모터 속도 제어 (자동/수동)
 │   ├─ sht20.cpp         # 온습도 센서 (I2C)
 │   ├─ audio.cpp         # WAV 재생 제어
 │   ├─ display.cpp       # 지하철 도착 정보 + 디스플레이 명령
 │   ├─ system_monitor.cpp# CPU/디스크/온도 모니터링
 │   ├─ motor_driver.c    # 커널 모듈: 모터 PWM/GPIO
 │   └─ sht20_driver.c    # 커널 모듈: SHT20 I2C
 ├─ includes/         # 헤더 파일
 ├─ audio/            # 오디오 UART 통신
 ├─ mic/              # 마이크 관련 처리
 ├─ display/          # 디스플레이 모듈
 ├─ subway_api/       # 서울 지하철 Open API 파싱
 ├─ config/           # 설정 파일 (config.json, gitignore됨)
 ├─ Makefile
 ├─ build.sh
 └─ setup_drivers.sh
```

---

## 🤍 4. 빌드 및 실행 가이드

### 1. 환경 설정 (최초 1회)

실행에 필요한 라이브러리와 드라이버 빌드 환경을 설치합니다.

```bash
chmod +x setup_drivers.sh
./setup_drivers.sh
```

설치 항목:
* `build-essential`, `raspberrypi-kernel-headers`, `libi2c-dev`, `nlohmann-json3-dev`, `libssl-dev`
* Paho MQTT C / C++ 라이브러리 (소스 빌드)
* I2C 활성화 여부 확인

- `build-essential`
- Linux kernel headers
- `libi2c-dev`
- `nlohmann-json3-dev`
- Eclipse Paho MQTT C / C++ libraries

Required Raspberry Pi interfaces:

- Enable `I2C`
- Enable `SPI`
- Keep the MCU UART available at `/dev/ttyS0`

If needed, use `sudo raspi-config` to enable `I2C` and `SPI`.

## 2. Configure `config/config.json`

Set the broker address, topics, and subway API key for your environment.

```json
{
  "mqtt_broker": "tcp://<server-ip>:1883",
  "subway_api_key": "<seoul-open-api-key>",
  "sensor_topic": "sensor/air_quality",
  "sht20_topic": "sensor/temp_humi"
}
```

> **참고 1:** `mqtt_broker`는 사용자 환경(서버 IP/포트)에 맞게 반드시 수정해야 합니다.
> **참고 2:** `subway_api_key`는 **실시간 지하철 위치정보 Open API** 인증키를 사용해야 합니다.
> **참고 3:** 인증키는 **서울 열린데이터광장**(https://data.seoul.go.kr/)에서 발급받을 수 있습니다.

Build the kernel modules and the application.

```bash
make all
```

The application binary is created at `build/drivers`.

## 4. Load kernel modules

`build.sh` still handles the module load and device permission setup.

```bash
chmod +x build.sh
./build.sh
```

It performs:

1. `make all`
2. `insmod src/motor_driver.ko`
3. `insmod src/sht20_driver.ko`
4. Permission setup for `/dev/motor`, `/dev/sht20`, `/dev/ttyS0`

## 5. Run

Start the integrated service:

```bash
./build/drivers
```

At startup, the process now runs these features together:

- MQTT communication for motor, sensor, display, and WAV control
- UART communication with the STM32 on `/dev/ttyS0`
- SHT20 monitoring
- System status publishing
- Qt -> Raspberry Pi -> MCU audio streaming server

* 스크립트 실행 권한 오류 발생 시 `chmod +x *.sh`를 입력하여 권한을 부여할 수 있습니다.

* 커널 모듈 로드 상태 확인: `lsmod | grep -E "motor_driver|sht20"`

* 디바이스 파일 확인: `ls /dev/motor /dev/sht20`

* 커널 모듈 수동 제거: `sudo rmmod motor_driver sht20_driver`

* **설정 파일** `config/config.json`에는 네트워크 정보와 API 인증키가 포함되므로 외부에 유출되지 않도록 주의해 주세요.
