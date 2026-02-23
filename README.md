

# 🚉 저기어때?

**지하철 승강장 구역별 혼잡도 안내 및 환경 관리 시스템** 
> 실시간 인원 카운팅을 통해 승객을 분산시키고, 역사 내 공기 질을 관리하는 지능형 관제 솔루션입니다.

---

## 🤍 1. 기획 의도

* **문제 의식** 

  유동인구가 많은 시간대, 계단 근처 등 특정 구역에만 승객이 밀집하여 열차 지연 및 안전사고 위험 발생
* **해결 방안**

  실시간 영상 분석을 통해 승강장 내 인원 밀집도를 파악하고, 상대적으로 여유로운 구역으로 승객 유도
* **기대 효과**

  열차 칸별 혼잡도 균형 유지, 열차 지연 예방, 쾌적하고 안전한 역사 환경 조성

---

## 🤍 2. 주요 기능

### 📊 혼잡도 분석 및 안내

* **실시간 스트리밍 & 모니터링** 

  관제 시스템에서 CCTV 영상 실시간 확인
* **AI 기반 인구 밀집도 계산** 

  YOLO26 및 CCTV 내장 지능형 분석을 활용해 구역별 인원수 검출
* **구역별 혼잡도 가이드** 

  산출된 밀집도(Low/Mid/High)를 LED Matrix 및 전용 디스플레이에 시각화하여 승객 유도

  특정 상황 발생 시 SD 카드에 저장된 음원(WAV)을 I2S 인터페이스를 통해 출력
* **이용 통계**

  요일/시간별 누적 데이터를 MariaDB에 저장하여 인사이트 제공

### 🌿 환경 관리 시스템

* **공기 질 측정** 

  유해가스(MQ-135) 및 일산화탄소(MQ-7) 센서 데이터 수집
* **자동 환기 제어**
  
  설정된 임계값 초과 시 DC 모터(팬)를 가동하여 정화
* **음성 안내** 

  특정 상황 발생 시 SD 카드에 저장된 음원(WAV)을 I2S 인터페이스를 통해 출력

---

## 🤍 3. 시스템 아키텍처

### 🛰️ Hardware & Environment

* **Vision:** Hanwha Vision PNO-A9081R, RPi Camera Module 2
* **Main Server:** Raspberry Pi 4 (4GB)
* **MCU Node:** STM32 NUCLEO-F401RE
* **Peripherals:** P5 LED Matrix (HUB75), MAX98357 (I2S), MQ-7/135, DC Motor

### 💻 Software Stack

* **AI/Vision:** NCNN, YOLO26
* **Server:** C++, OpenCV, MQTT (Paho), OpenSSL, MariaDB/SQLite
* **Client:** Windows Application (Qt 6.10.0)
* **Embedded:** HAL Driver, UART/SPI/I2S Communication

---

## 🤍 4. 구현 상세 (Implementation)

### 📸 Camera & Vision

* **Heterogeneous Camera Support:** 한화비전 WiseAI(군중 카운팅 메타데이터)와 일반 RPi 카메라(YOLO26 기반 자체 분석) 동시 지원
* **Parallel Processing:** FFmpeg 기반 영상 수신 및 처리

### 🛡️ Security & Network

* **Secure Communication:** 중앙 서버와 Qt 클라이언트 간 데이터 보호를 위한 **OpenSSL(TLS/SSL)** 적용
* **Messaging:** MQTT 브로커를 통한 비동기 데이터 취합 및 JSON 파싱 처리

### ⚙️ Firmware (STM32)

* **Multi-Tasking:** LED 리프레시, 센서 데이터 수집, 오디오 재생 태스크를 관리
* **Storage:** SPI 인터페이스 기반 SD Card 연동으로 음원 파일 관리

---

## 📂 프로젝트 구조 (Repository Structure)

```text
repo/
 ├─ server/           # 중앙 관리 서버 (C++, API, DB, MQTT)
 ├─ embedded-linux/   # 카메라 노드 (YOLO, GStreamer, MQTT)
 ├─ firmware/         # STM32 펌웨어
 ├─ client/           # Qt 관리자용 어플리케이션
 ├─ drivers/          # 커스텀 장치 드라이버
 └─ docs/             # 설계서 및 프로토콜 정의서

```

---

## 📅 개발 일정

* **기간:** 2026.01.27 ~ 2026.04.01 (약 9주)
* **주요 마일스톤:**
  * 1~2주: 요구사항 분석 및 시스템 설계
  * 3~5주: 모듈별 기능 구현 (AI 모듈, 서버-클라이언트 통신)
  * 6~8주: 하드웨어 통합 및 최적화
  * 9주: 테스트 및 최종 문서화



---
