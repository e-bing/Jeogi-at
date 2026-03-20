# 🔌 STM32F401 엣지 디바이스 펌웨어

**지하철 승강장 구역별 혼잡도 안내 및 환경 관리 시스템 (저기어때?)**의 펌웨어 모듈입니다.  
STM32F401RE 기반으로 센서/디스플레이/오디오 주변장치를 제어하며, 상위 시스템과 연동되는 엣지 장치 동작을 담당합니다.

---

## 🤍 1. 주요 기능

* **UART Debug**
* **Multi Sensor 연동**
* **MQ7 (CO) / MQ135 가스 센서 측정 (ADC, EMA 필터)**
* **SDCard Read (SPI)**
* **MAX98357A AMP Class D Amplifier (I2S)**
* **LED Matrix (HUB75)**

---

## 🤍 2. 시스템 환경 및 빌드 스택

* **Target MCU:** STM32F401RE (Cortex-M4, FPU)
* **IDE:** STM32CubeIDE 2.0.0
* **CubeMX:** 6.8.0
* **Build System:** CMake 3.20+
* **Toolchain:** GNU Arm Embedded Toolchain (`arm-none-eabi-gcc`)
* **권장 OS:** Ubuntu 22.04 / 24.04 LTS

> 참고: `.ioc` 파일을 열 때 CubeMX 버전에 따라 코드 생성 옵션/핀 설정 표시가 달라질 수 있으므로 `6.8.0` 기준 사용을 권장합니다.

---

## 🤍 3. 프로젝트 구조

| 구분 | 경로 / 파일 | 설명 |
|------|-------------|------|
| IDE 설정 | `.project`<br>`.cproject`<br>`.mxproject`<br>`.settings/` | STM32CubeIDE / Eclipse 프로젝트 설정 |
| 하드웨어 설정 | `*.ioc` | 핀맵, 클럭, 주변장치 설정 (CubeMX) |
| 사용자 코드 | `Core/`<br>`Startup/` | 펌웨어 메인 소스 코드 및 시작 코드 |
| 드라이버 | `Drivers/` | CMSIS / HAL 라이브러리 |
| 미들웨어 | `Middlewares/`, `FATFS/` | 파일시스템 및 기타 미들웨어 |
| 메모리 설정 | `*.ld` | Flash / RAM 메모리 배치 |
| 디버그 설정 | `STM32F401_EdgeDevice Debug.launch` | 디버그 런치 설정 |
| 빌드 출력 | `build_cmake/` | CMake 빌드 산출물 |

---

## 🤍 4. 빌드 가이드 (Build)

### 1. 사전 준비 (CLI 빌드 기준)

아래 도구들이 설치되어 있어야 합니다.

* `cmake` (3.20 이상)
* `make`
* `arm-none-eabi-gcc`
* `arm-none-eabi-objcopy`, `arm-none-eabi-size`
* ST-LINK (플래시/디버깅 시)

Ubuntu 설치 예시:

```bash
sudo apt update
sudo apt install -y cmake make gcc-arm-none-eabi binutils-arm-none-eabi
```

### 2. STM32CubeIDE 빌드 (추천)

```text
1) STM32CubeIDE에서 firmware/STM32F401_EdgeDevice 프로젝트 열기
2) Build Config를 Debug 또는 Release로 선택
3) Project > Build Project 실행
```

### 3. CMake CLI 빌드

```bash
cd firmware/STM32F401_EdgeDevice
cmake -S . -B build_cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build_cmake -j
```

### 4. 빌드 산출물

* ELF: `build_cmake/STM32F401_EdgeDevice`
* MAP: `build_cmake/STM32F401_EdgeDevice.map`
* HEX: `build_cmake/STM32F401_EdgeDevice.hex`
* BIN: `build_cmake/STM32F401_EdgeDevice.bin`

---

## 🤍 5. 플래시/디버그

* `STM32F401_EdgeDevice Debug.launch`를 사용해 CubeIDE에서 디버그 실행 가능
* CLI 기반 플래시가 필요하면 OpenOCD 또는 STM32CubeProgrammer를 별도 설치해 사용

---

## 🤍 6. 트러블슈팅

* `arm-none-eabi-gcc: command not found`
  * GNU Arm Toolchain 미설치 또는 PATH 미등록 상태입니다.

* CMake 버전 관련 오류
  * `cmake_minimum_required(VERSION 3.20)` 조건을 만족하는 버전인지 확인해 주세요.

* 링커 스크립트 관련 오류
  * `STM32F401RETX_FLASH.ld` 경로가 프로젝트 루트 기준으로 올바른지 확인해 주세요.
