## STM32F401 Edge Device

### MCU
STM32F401RE

### Development Tool
STM32CubeIDE 2.0.0
STM32CubeMX  6.8.0

### External Host
Raspberry Pi  
- UART Communication with STM32
- SPI Communication with STM32

### Features
- Raspberry Pi Interface
  - Command Protocol (UART)
  - Audio Streaming (SPI)
- Multi Sensor (ADC)
- SD Card Read (SPI)
- MAX98357A Class-D Amplifier (I2S)
- LED Matrix (HUB75)
- Debug (UART)

---

## System Overview

This project is based on the STM32F401RE MCU and operates as an edge device connected to a Raspberry Pi.  
The Raspberry Pi communicates with the STM32 through UART and SPI interfaces for data exchange and control.

### Communication Interfaces
- **UART**: Debug logging and communication with Raspberry Pi
- **SPI**: High-speed data exchange with Raspberry Pi and SD Card access
- **I2S**: Audio output to MAX98357A amplifier
- **GPIO / HUB75**: LED Matrix control

---

## Project File Structure

| Category | Path / File | Description |
|----------|-------------|-------------|
| IDE Config | `.project`<br>`.cproject`<br>`.mxproject`<br>`.settings/` | STM32CubeIDE / Eclipse project configuration files |
| Hardware Config | `*.ioc` | Pin mapping, clock configuration, and peripheral settings (CubeMX) |
| User Code | `Core/`<br>`Startup/` | Application source code and startup files |
| Drivers | `Drivers/` | STM32 CMSIS and HAL driver libraries |
| Linker Script | `*.ld` | Flash/RAM memory layout configuration |
| Git Config | `.gitignore` | Excludes build artifacts and temporary files |

---

## 프로젝트 개요

이 프로젝트는 **STM32F401RE** 기반의 엣지 디바이스 펌웨어이며,  
**Raspberry Pi와 UART 및 SPI 인터페이스로 연결**되어 데이터 송수신과 제어를 수행합니다.

### 통신 인터페이스
- **UART**: 디버그 출력 및 Raspberry Pi와의 시리얼 통신
- **SPI**: Raspberry Pi와의 고속 통신 및 SD Card 접근
- **I2S**: MAX98357A 앰프 오디오 출력
- **GPIO / HUB75**: LED Matrix 제어

---

## 프로젝트 파일 구조

| 구분 | 경로 / 파일 | 설명 |
|------|-------------|------|
| IDE 설정 | `.project`<br>`.cproject`<br>`.mxproject`<br>`.settings/` | STM32CubeIDE 프로젝트 설정 |
| 하드웨어 설정 | `*.ioc` | 핀맵, 클럭, 주변장치 설정 |
| 사용자 코드 | `Core/`<br>`Startup/` | 펌웨어 메인 소스 코드 |
| 드라이버 | `Drivers/` | CMSIS / HAL 라이브러리 |
| 메모리 설정 | `*.ld` | Flash / RAM 메모리 배치 |
| Git 설정 | `.gitignore` | 빌드 산출물 및 임시 파일 제외 |

---

## Hardware Connection Summary

- **Raspberry Pi ↔ STM32**
  - UART: command-based control protocol
  - SPI: high-speed data transfer
- **STM32 ↔ SD Card**
  - SPI interface
- **STM32 ↔ MAX98357A**
  - I2S audio output
- **STM32 ↔ LED Matrix**
  - HUB75 interface