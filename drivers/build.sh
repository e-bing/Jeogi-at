#!/bin/bash
set -e

# sudo 없이 실행하면 자동으로 sudo로 재실행
if [ "$EUID" -ne 0 ]; then
    exec sudo "$0" "$@"
fi

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
SRC_DIR="$SCRIPT_DIR/src"

# 1. 프로젝트 빌드
echo "[1/3] 프로젝트 빌드 중..."
cd "$SCRIPT_DIR"
make all
echo "✅ 빌드 완료"

# 2. 커널 모듈 로드
echo ""
echo "[2/3] 커널 모듈 로드 중..."
sudo insmod "$SRC_DIR/motor_driver.ko" 2>/dev/null || echo "⚠️ 모터 드라이버 이미 로드됨 (정상)"
echo "✅ 모터 드라이버 로드 완료"

sudo insmod "$SRC_DIR/sht20_driver.ko" 2>/dev/null || echo "⚠️ SHT20 드라이버 이미 로드됨 (정상)"
echo "✅ SHT20 드라이버 로드 완료"

# 3. 디바이스 설정
echo ""
echo "[3/3] 디바이스 설정 중..."
if [ ! -e /dev/sht20 ]; then
    echo sht20 0x40 | sudo tee /sys/class/i2c-adapter/i2c-1/new_device > /dev/null
fi

sudo chmod 666 /dev/motor 2>/dev/null && echo "✅ /dev/motor 권한 설정 완료"
sudo chmod 666 /dev/sht20 2>/dev/null && echo "✅ /dev/sht20 권한 설정 완료"
sudo chmod 666 /dev/ttyS0 2>/dev/null && echo "✅ /dev/ttyS0 권한 설정 완료"

echo ""
echo "=============================================="
echo " ✅ 빌드 및 insmod 완료! ./build/drivers로 실행하세요!"
echo "=============================================="
