#!/bin/bash
# 라즈베리파이 서버 통합 설치 스크립트 v2.1 (경로 및 보안 설정 수정본)

echo "--- 1. 패키지 시스템 잠금 해제 및 복구 ---"
sudo rm /var/lib/dpkg/lock-frontend 2>/dev/null
sudo rm /var/lib/apt/lists/lock 2>/dev/null
sudo dpkg --configure -a
sudo apt --fix-broken install -y

echo "--- 2. 시스템 업데이트 ---"
sudo apt update -y

echo "--- 3. MariaDB(MySQL) 설치 및 경로 복구 ---"
# 누락된 설정 디렉토리 미리 생성
sudo mkdir -p /etc/mysql/conf.d/ /etc/mysql/mariadb.conf.d/
sudo apt install -y mariadb-server
sudo systemctl enable mariadb
sudo systemctl start mariadb

echo "--- 4. MariaDB 보안 설정 적용 ---"
# 이전 코드에서 잘렸던 부분을 정확하게 수정했습니다.
sudo mysql -e "DELETE FROM mysql.user WHERE User='';"
sudo mysql -e "DELETE FROM mysql.user WHERE User='root' AND Host NOT IN ('localhost', '127.0.0.1', '::1');"
sudo mysql -e "DROP DATABASE IF EXISTS test;"
sudo mysql -e "DELETE FROM mysql.db WHERE Db='test' OR Db='test\\_%';"
sudo mysql -e "FLUSH PRIVILEGES;"

echo "--- 5. MQTT 브로커(Mosquitto) 설치 ---"
sudo apt install -y mosquitto mosquitto-clients
sudo systemctl enable mosquitto
sudo systemctl start mosquitto

echo "--- 6. MQTT 외부 접속 허용 설정 (0.0.0.0) ---"
sudo mkdir -p /etc/mosquitto/conf.d/
sudo bash -c 'cat > /etc/mosquitto/conf.d/default.conf <<EOF
listener 1883 0.0.0.0
allow_anonymous true
EOF'

echo "--- 7. 서비스 최종 재시작 및 확인 ---"
sudo systemctl restart mosquitto
sudo systemctl restart mariadb

echo "--------------------------------------"
echo "설치 완료!"
echo "DB 접속 확인: sudo mysql"
echo "MQTT 포트 상태 (1883 확인):"
sudo ss -tuln | grep 1883
echo "--------------------------------------"
