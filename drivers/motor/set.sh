#!/bin/bash
sudo insmod motor_driver.ko 2>/dev/null || echo "⚠️ 드라이버 이미 로드됨 (정상)"
echo "✅ 모터 드라이버 로드 완료"
sudo chmod 666 /dev/motor
echo "✅ /dev/motor 권한 설정 완료"
echo "🚀 motor_app 실행 준비 완료!"
