#!/bin/bash

# 1. 서버의 실제 IP 자동 추출
SERVER_IP=$(hostname -I | awk '{print $1}')

# 만약 IP를 못 찾았을 경우 대비 (fallback)
if [ -z "$SERVER_IP" ]; then
    SERVER_IP="127.0.0.1"
fi

echo "-------------------------------------------"
echo "감지된 서버 IP: $SERVER_IP"
echo "-------------------------------------------"

# 2. 저장할 디렉토리 설정
TARGET_DIR="config"
mkdir -p $TARGET_DIR

echo "인증서 생성 및 $TARGET_DIR/ 폴더 이동 중..."

# 3. 인증서 생성 (직접 config/ 경로에 생성)
openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
  -keyout "$TARGET_DIR/server.key" -out "$TARGET_DIR/server.crt" \
  -subj "/CN=$SERVER_IP" \
  -addext "subjectAltName = IP:$SERVER_IP, IP:127.0.0.1"

echo "-------------------------------------------"
echo "생성 완료:"
ls -l "$TARGET_DIR/server.crt" "$TARGET_DIR/server.key"
echo "-------------------------------------------"
echo "이제 Main.qml의 serverIp를 '$SERVER_IP'로 수정하세요."