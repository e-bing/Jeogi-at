#!/bin/bash

# 1. 모터 드라이버 로드
sudo insmod motor_driver.ko 2>/dev/null || echo "⚠️ 드라이버 이미 로드됨 (정상)"
echo "✅ 모터 드라이버 로드 완료"

# 2. SHT20 드라이버 로드
sudo insmod sht20_driver.ko 2>/dev/null || echo "⚠️ 드라이버 이미 로드됨 (정상)"
echo "✅ SHT20 드라이버 로드 완료"

# 3. I2C 장치 강제 인식 (이 줄이 추가되어야 /dev/sht20이 생깁니다)
if [ ! -e /dev/sht20 ]; then
    echo sht20 0x40 | sudo tee /sys/class/i2c-adapter/i2c-1/new_device > /dev/null
fi

# 4. 권한 설정
sudo chmod 666 /dev/motor 2>/dev/null && echo "✅ /dev/motor 권한 설정 완료"
sudo chmod 666 /dev/sht20 2>/dev/null && echo "✅ /dev/sht20 권한 설정 완료"

echo "🚀 motor_app 실행 준비 완료!"
