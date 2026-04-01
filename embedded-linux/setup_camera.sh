#!/bin/bash

# 에러 발생 시 중단
set -e

echo "-------------------------------------------------------"
echo "라즈베리파이 개발 환경 통합 설정 스크립트 시작"
echo "대상: OpenCV, Paho MQTT, ncnn, 카메라 권한"
echo "-------------------------------------------------------"

# 1. 시스템 업데이트 및 필수 도구 설치
sudo apt update
sudo apt install -y build-essential cmake git libssl-dev libopencv-dev \
                    libcamera-dev nlohmann-json3-dev mosquitto g++

# 2. Paho MQTT C 설치 (버전 충돌 방지를 위해 /usr에 설치)
echo "[1/3] Paho MQTT C 라이브러리 설치 중..."
cd ~
if [ -d "paho.mqtt.c" ]; then rm -rf paho.mqtt.c; fi
git clone https://github.com/eclipse/paho.mqtt.c.git
cd paho.mqtt.c
mkdir build && cd build
cmake .. -DPAHO_WITH_SSL=ON -DPAHO_ENABLE_TESTING=OFF -DCMAKE_INSTALL_PREFIX=/usr
make -j$(nproc)
sudo make install
sudo ldconfig

# 3. Paho MQTT C++ 설치
echo "[2/3] Paho MQTT C++ 라이브러리 설치 중..."
cd ~
if [ -d "paho.mqtt.cpp" ]; then rm -rf paho.mqtt.cpp; fi
git clone https://github.com/eclipse/paho.mqtt.cpp.git
cd paho.mqtt.cpp
mkdir build && cd build
cmake .. -DPAHO_BUILD_STATIC=ON -DPAHO_BUILD_SHARED=ON -DCMAKE_INSTALL_PREFIX=/usr
make -j$(nproc)
sudo make install
sudo ldconfig

# 4. ncnn 설치
echo "[3/3] ncnn 라이브러리 설치 중..."
cd ~
if [ -d "ncnn" ]; then rm -rf ncnn; fi
git clone https://github.com/Tencent/ncnn.git
cd ncnn
git submodule update --init
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DNCNN_VULKAN=OFF -DNCNN_BUILD_EXAMPLES=OFF ..
make -j$(nproc)
sudo make install
sudo ldconfig

# 5. 카메라 권한 설정
echo "-------------------------------------------------------"
echo "카메라 및 비디오 장치 접근 권한 부여 중..."
sudo usermod -aG video $USER
sudo usermod -aG render $USER  # GPU 가속 관련 권한

# udev 규칙 추가 (재부팅 없이도 동작 확률 높임)
echo 'SUBSYSTEM=="video4linux", GROUP="video", MODE="0660"' | sudo tee /etc/udev/rules.d/99-camera.rules
sudo udevadm control --reload-rules && sudo udevadm trigger

echo "-------------------------------------------------------"
echo "모든 설정이 완료되었습니다!"
echo "권한 적용을 위해 반드시 'sudo reboot' 명령어로 재부팅해 주세요."
echo "-------------------------------------------------------"