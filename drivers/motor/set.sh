#!/bin/bash
sudo insmod motor_driver.ko
echo "모터 드라이버 로드 완료!"
sudo chmod 666 /dev/motor
echo "/dev/motor 권한 설정 완료!"
echo "motor 가동 준비 완료!!!"
