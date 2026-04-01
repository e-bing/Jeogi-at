#!/bin/bash
# 라즈베리파이 drivers 통합 설치 스크립트
set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

echo "========================================="
echo " Jeogi-at drivers 환경 설정 시작"
echo "========================================="

# ─────────────────────────────────────────────
# 1단계: apt 패키지 설치
# ─────────────────────────────────────────────
echo ""
echo "[1/5] apt 패키지 설치 중..."

sudo apt-get update -qq
sudo apt-get install -y \
    build-essential \
    raspberrypi-kernel-headers \
    libi2c-dev \
    nlohmann-json3-dev \
    libssl-dev \
    cmake \
    git

echo ":white_check_mark: apt 패키지 설치 완료"

# ─────────────────────────────────────────────
# 2단계: Paho MQTT C 라이브러리 설치
# ─────────────────────────────────────────────
echo ""
echo "[2/5] Paho MQTT C 라이브러리 확인 중..."

if ldconfig -p | grep -q libpaho-mqtt3as; then
    echo ":white_check_mark: Paho MQTT C 이미 설치됨 (스킵)"
else
    echo "  → 소스에서 빌드합니다..."
    cd /tmp
    rm -rf paho.mqtt.c
    git clone https://github.com/eclipse/paho.mqtt.c.git
    cd paho.mqtt.c && mkdir build && cd build
    cmake .. -DPAHO_WITH_SSL=ON -DPAHO_ENABLE_SAMPLES=OFF
    make -j$(nproc) && sudo make install
    cd "$SCRIPT_DIR"
    echo ":white_check_mark: Paho MQTT C 설치 완료"
fi

# ─────────────────────────────────────────────
# 3단계: Paho MQTT C++ 라이브러리 설치
# ─────────────────────────────────────────────
echo ""
echo "[3/5] Paho MQTT C++ 라이브러리 확인 중..."

if ldconfig -p | grep -q libpaho-mqttpp3; then
    echo ":white_check_mark: Paho MQTT C++ 이미 설치됨 (스킵)"
else
    echo "  → 소스에서 빌드합니다..."
    cd /tmp
    rm -rf paho.mqtt.cpp
    git clone https://github.com/eclipse/paho.mqtt.cpp.git
    cd paho.mqtt.cpp && mkdir build && cd build
    cmake .. && make -j$(nproc) && sudo make install
    sudo sed -i 's/add_library(PahoMqttCpp::paho-mqttpp3 ALIAS PahoMqttCpp::paho-mqttpp3-shared)/set_target_properties(PahoMqttCpp::paho-mqttpp3-shared PROPERTIES IMPORTED_GLOBAL TRUE)\n    add_library(PahoMqttCpp::paho-mqttpp3 ALIAS PahoMqttCpp::paho-mqttpp3-shared)/' /usr/local/lib/cmake/PahoMqttCpp/PahoMqttCppConfig.cmake
    sudo ldconfig
    cd "$SCRIPT_DIR"
    echo ":white_check_mark: Paho MQTT C++ 설치 완료"
fi

# ─────────────────────────────────────────────
# 4단계: I2C 활성화 확인
# ─────────────────────────────────────────────
echo ""
echo "[4/5] I2C 활성화 확인 중..."

I2C_ENABLED=false
for cfg in /boot/config.txt /boot/firmware/config.txt; do
    if [ -f "$cfg" ] && grep -q "dtparam=i2c_arm=on" "$cfg"; then
        I2C_ENABLED=true
        break
    fi
done

if $I2C_ENABLED || [ -e /dev/i2c-1 ]; then
    echo ":white_check_mark: I2C 활성화됨"
else
    echo ":warning:  I2C가 비활성화되어 있습니다."
    echo "   다음 명령으로 활성화하세요: sudo raspi-config → Interface Options → I2C → Enable"
    echo "   또는 /boot/firmware/config.txt (또는 /boot/config.txt) 에 아래 줄 추가 후 재부팅:"
    echo "    dtparam=i2c_arm=on"
    echo ""
    read -r -p "   계속 진행하시겠습니까? [y/N] " answer
    if [[ ! "$answer" =~ ^[Yy]$ ]]; then
        echo "setup.sh 중단. I2C 활성화 후 다시 실행하세요."
        exit 1
    fi
fi

# ─────────────────────────────────────────────
# 5단계: PWM 오버레이 확인
# ─────────────────────────────────────────────
echo ""
echo "[5/5] PWM 오버레이 확인 중..."

PWM_ENABLED=false
for cfg in /boot/config.txt /boot/firmware/config.txt; do
    if [ -f "$cfg" ] && grep -q "dtoverlay=pwm" "$cfg"; then
        PWM_ENABLED=true
        break
    fi
done

if $PWM_ENABLED; then
    echo ":white_check_mark: PWM 오버레이 설정됨"
else
    echo "  → PWM 오버레이가 없습니다. /boot/config.txt에 추가합니다..."
    for cfg in /boot/config.txt /boot/firmware/config.txt; do
        if [ -f "$cfg" ]; then
            echo "dtoverlay=pwm-2chan" | sudo tee -a "$cfg" > /dev/null
            echo ":white_check_mark: PWM 오버레이 추가 완료 ($cfg) — 재부팅 후 적용됩니다"
            break
        fi
    done
fi

echo "=============================================="
echo " :white_check_mark: 의존성 설치 완료! ./build.sh 로 빌드 및 실행하세요."
echo "=============================================="