# 📸 카메라 및 AI 비전 처리 엣지 노드

**지하철 승강장 구역별 혼잡도 안내 및 환경 관리 시스템 (저기어때?)**
의 카메라 엣지 노드 모듈입니다.
라즈베리파이 환경에서 실시간으로 영상을 수집하고, YOLO26 Nano(NCNN)를 활용해 승강장 내 객체(사람)를 탐지한 후 그 데이터를 중앙 서버로 전송합니다.

---

## 🤍 1. 주요 기능

* **실시간 객체 탐지 (온디바이스 AI)**

  * 경량화된 NCNN 프레임워크와 YOLO26 모델을 활용해 라즈베리파이 내에서 사람을 실시간으로 탐지하고 인원수를 계산합니다.

  * YOLO26 모델은 사람 감지 전용 단일 클래스로 export되었고, FP16 최적화와 4스레드 추론을 적용하여 연산의 효율을 높였습니다. 

* **영상 전처리 및 화질 개선**

  * LAB/HSV 색상 보정, 명암비(Contrast) 개선, 샤프닝 필터 처리를 통해 카메라 모듈 특성에 맞는 영상 보정을 적용합니다.

* **비디오 스트리밍 송출**

  * GStreamer(`v4l2h264enc`) 하드웨어 가속 파이프라인을 구축하여, 실시간 영상을 H.264로 인코딩해 TCP 스트리밍(포트 5000)으로 제공합니다.

* **MQTT 기반 데이터 발행**

  * 탐지된 객체의 바운딩 박스 위치와 사람 수 데이터를 JSON 형태로 가공하여, MQTT 토픽(`iot/{device_id}/sensor/camera`)으로 중앙 서버에 발행합니다.

---

## 🤍 2. 시스템 환경 및 기술 스택

* **Hardware:** Raspberry Pi 4, Raspberry Pi Camera Module V2

* **Language:** C++17

* **Framework & Library:**

  * **Vision & AI:** OpenCV, NCNN (Tencent)

  * **Multimedia:** GStreamer (`libcamerasrc`)

  * **Network:** Eclipse Paho MQTT (C/C++), nlohmann/json

  * **Build System:** CMake

---

## 🤍 3. 폴더 구조

```text
embedded-linux/
 ├─ models/           # NCNN 추론용 모델 파일 (model.ncnn.bin, model.ncnn.param)
 ├─ src/
 │   └─ main.cpp      # 메인 소스 코드 (영상 캡처, 전처리, AI 추론, MQTT 전송)
 ├─ build.sh          # 빌드 자동화 쉘 스크립트
 ├─ setup_camera.sh   # 의존성 패키지/라이브러리 빌드 및 카메라 권한 설정 스크립트
 └─ CMakeLists.txt    # CMake 빌드 설정 파일
```

---

## 🤍 4. 설치 및 실행 방법

### 1. 환경 설정 (의존성 설치)

라즈베리파이에서 아래 스크립트를 실행하여 OpenCV, NCNN, Paho MQTT 등 필수 라이브러리를 소스 빌드하고, 카메라 접근 권한 및 udev 규칙을 설정합니다.

```bash
chmod +x setup_camera.sh
./setup_camera.sh
```

> 💡 **참고:** 스크립트 실행 완료 후, 비디오(Camera) 그룹 권한 적용을 위해 반드시 **재부팅(`sudo reboot`)**
이 필요합니다.

### 2. 프로젝트 빌드
제공된 `build.sh` 스크립트를 실행하여 프로젝트를 빌드합니다.

```bash
chmod +x build.sh
./build.sh

# 기존 빌드 파일을 지우고 새로 빌드하려면 아래 명령어를 사용합니다.
# ./build.sh clean
```

### 3. 노드 실행
빌드가 성공적으로 완료되면 `build` 폴더로 이동하여 실행 파일을 구동합니다.

```bash
cd build
./camera_node
```

> **스트리밍 확인 방법:**
> 실행 후 관제망 내의 다른 PC에서 VLC 미디어 플레이어(네트워크 스트림 열기) 등을 사용하여 `tcp://<라즈베리파이_IP>:5000` 주소로 접속하면 실시간 영상을 모니터링할 수 있습니다.