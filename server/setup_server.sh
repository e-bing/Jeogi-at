#!/bash/bin
# 라즈베리파이 서버 통합 설치 스크립트 (DB & MQTT)

echo "--- 1. 시스템 업데이트 시작 ---"
sudo apt update && sudo apt upgrade -y

echo "--- 2. MariaDB(MySQL) 설치 및 설정 ---"
sudo apt install -y mariadb-server
# MariaDB 기본 보안 설정 (비밀번호 없이 엔터로 접속 가능한 상태 유지)
# 기본적으로 Raspbian의 MariaDB는 'unix_socket' 방식을 써서 sudo mysql로 비번 없이 들어갑니다.
sudo systemctl enable mariadb
sudo systemctl start mariadb

echo "--- 3. MQTT 브로커(Mosquitto) 설치 ---"
sudo apt install -y mosquitto mosquitto-clients
sudo systemctl enable mosquitto

echo "--- 4. MQTT 외부 접속 허용 설정 (0.0.0.0) ---"
# 기존 설정을 무시하고 덮어쓰기 위해 새 파일을 생성합니다.
sudo bash -c 'cat > /etc/mosquitto/conf.d/default.conf <<EOF
listener 1883 0.0.0.0
allow_anonymous true
EOF'

echo "--- 5. 서비스 재시작 및 상태 확인 ---"
sudo systemctl restart mosquitto
sudo systemctl restart mariadb

echo "--- 설치 완료! ---"
echo "DB 접속: sudo mysql"
echo "MQTT 포트 상태:"
sudo ss -tuln | grep 1883
