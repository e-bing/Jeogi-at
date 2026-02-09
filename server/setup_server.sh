#!/bin/bash
# 라즈베리파이 서버 통합 설치 스크립트 v3.0

echo "--- 1. 패키지 시스템 잠금 해제 및 복구 ---"
sudo rm /var/lib/dpkg/lock-frontend 2>/dev/null
sudo rm /var/lib/apt/lists/lock 2>/dev/null
sudo dpkg --configure -a
sudo apt --fix-broken install -y

echo "--- 2. 시스템 업데이트 및 필수 빌드 도구 설치 ---"
sudo apt update -y
sudo apt install -y build-essential cmake git pkg-config

echo "--- 3. MariaDB(MySQL) 설치 및 경로 복구 ---"

sudo mkdir -p /etc/mysql/conf.d/ /etc/mysql/mariadb.conf.d/
sudo apt install -y mariadb-server
sudo systemctl enable mariadb
sudo systemctl start mariadb

echo "--- 4. MariaDB 보안 설정 적용 ---"

sudo mysql -e "DELETE FROM mysql.user WHERE User='';"
sudo mysql -e "DELETE FROM mysql.user WHERE User='root' AND Host NOT IN ('localhost', '127.0.0.1', '::1');"
sudo mysql -e "DROP DATABASE IF EXISTS test;"
sudo mysql -e "DELETE FROM mysql.db WHERE Db='test' OR Db='test\\_%';"
sudo mysql -e "FLUSH PRIVILEGES;"

echo "--- 5. MQTT Client(Mosquitto) 설치 ---"
sudo apt install -y libssl-dev git
cd /tmp
# Paho MQTT C 설치
git clone https://github.com/eclipse/paho.mqtt.c.git
cd paho.mqtt.c && mkdir build && cd build
cmake .. -DPAHO_WITH_SSL=ON -DPAHO_ENABLE_SAMPLES=OFF
make -j$(nproc) && sudo make install

# Paho MQTT C++ 설치
cd /tmp
git clone https://github.com/eclipse/paho.mqtt.cpp.git
cd paho.mqtt.cpp && mkdir build && cd build
cmake .. && make -j$(nproc) && sudo make install
sudo ldconfig

echo "--- 6. 영상 처리/디코딩/시각화 라이브러리 설치 ---"
# 서버에서 cctv_app(FFmpeg + SDL2 + OpenCV)을 돌리기 위한 필수 요소
sudo apt install -y libavformat-dev libavcodec-dev libavutil-dev libswscale-dev \
                   libsdl2-dev libopencv-dev build-essential cmake nlohmann-json3-dev

echo "--- 7. 서비스 최종 재시작 및 확인 ---"
sudo systemctl restart mariadb

echo "--------------------------------------"
echo "설치 완료!"
echo "DB 접속 확인: sudo mysql"
echo "MQTT 포트 상태 (1883 확인):"
sudo ss -tuln | grep 1883
echo "--------------------------------------"
