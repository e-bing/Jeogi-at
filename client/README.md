# 💻 관리자용 대시보드 UI 클라이언트

**지하철 승강장 구역별 혼잡도 안내 및 환경 관리 시스템 (저기어때?)**의 관제용 Windows 데스크톱 애플리케이션입니다.
관리자는 본 애플리케이션을 통해 메인 서버에 접속하여 실시간 카메라 스트리밍, 승강장 구역별 혼잡도 현황, 공기질 및 온/습도 센서 데이터를 한눈에 모니터링할 수 있으며, 환기팬(모터), 오디오 방송, 전광판 제어 등 역사 내 환경을 원격으로 관리할 수 있습니다.

---

## 🤍 1. 주요 기능

* **대시보드 모니터링**

  * 여러 대의 카메라(Hanwha 및 Raspberry Pi 카메라)에서 전송되는 실시간 영상을 격자 뷰로 확인

  * 실시간 영상 녹화 기능 및 이전 녹화 기록 조회(Playback) 기능 제공

* **혼잡도 및 환경 데이터 시각화**

  * 서버에서 분석된 구역별 혼잡도(원활, 보통, 혼잡) 상태를 시각적인 UI로 제공

  * 수집된 센서 데이터(유해가스, CO, 온/습도)를 실시간 차트/수치로 표기하여 역사 환경 상태 파악

* **원격 장치 제어**

  * 디스플레이(전광판), 오디오 방송 출력, 환기 모터(DC 팬) 수동/자동 제어

  * 필요시 마이크 스트리밍 기능을 통한 관리자 실시간 안내 방송 송출 기능 제공

* **보안 통신 연동**

  * 서버와의 통신 시 TLS/SSL 암호화 연결을 통해 제어 명령 및 스트리밍 데이터의 안전성 보장

---

## 🤍 2. 시스템 환경 및 기술 스택

* **Language:** C++17, QML
* **Framework & Library:**
  * **UI & Core:** Qt 6 (Qt Quick, Qt QML)
  * **Network & Security:** Qt Network, OpenSSL (TLS 소켓 통신)
  * **Multimedia:** Qt Multimedia, FFmpeg (영상 녹화 및 외부 프로세스 처리용)
* **Build System:** qmake (`.pro`)
* **Target OS:** Windows (win32/x64)

---

## 🤍 3. 폴더 구조

```text
client/
 ├─ assets/             # README용 이미지
 ├─ src/
 │   ├─ backend/        # C++ 코어 로직 (NetworkClient, RecordingManager 등)
 │   ├─ components/     # 재사용 가능한 QML UI 컴포넌트
 │   ├─ pages/          # 개별 화면 QML (대시보드, 통계, 디바이스 제어 등)
 │   ├─ config/         # 서버 인증서(server.crt) 보관 폴더
 │   ├─ fonts/          # UI용 폰트 파일
 │   ├─ logo/           # 프로젝트 및 UI 로고 이미지
 │   ├─ record/         # 카메라별 로컬 녹화 영상 저장 경로
 │   ├─ main.cpp        # Qt 애플리케이션 진입점 및 C++ 객체 QML 바인딩
 │   ├─ Main.qml        # 애플리케이션 메인 윈도우 UI
 │   ├─ qml.qrc         # 리소스 컴파일 설정 파일
 │   └─ metro_smart_subway.pro  # qmake 프로젝트 파일
 └─ README.md           # README 파일
```

---

## 🤍 4. 빌드 및 실행 가이드

클라이언트 앱을 정상적으로 구동하기 위해서는 **서버 인증서 등록**과 영상 녹화/처리를 위한 **FFmpeg 설치**가 필수적입니다.

### 1. 서버 인증서 (`server.crt`) 설정

클라이언트는 서버와 안전한 TLS 암호화 통신을 합니다. 
1. 메인 서버 구축 시 생성된 `server.crt` 파일을 복사하여 클라이언트 측의 아래 경로에 위치시킵니다.

   ```text
   client/src/config/server.crt
   ```

> 💡 **참고:** 인증서 파일이 없거나 접속하려는 서버의 IP가 인증서의 정보와 불일치할 경우, 서버 접속이 거부됩니다.

### 2. FFmpeg 설치

영상 녹화 기능을 사용하기 위해 시스템에 FFmpeg가 설치되어 있어야 합니다.

---

1. **다운로드:** 

   [FFmpeg Windows 빌드 다운로드 페이지](https://www.gyan.dev/ffmpeg/builds/)에 접속하여 

   `ffmpeg-git-full.7z` 파일을 다운로드합니다.

   <br><img width="60%" src="assets/ffmpeg_1.png" alt="ffmpeg 다운로드">

2. **압축 해제:** 

   다운로드한 파일의 압축을 해제하고 원하는 시스템 경로(예: `C:\ffmpeg`)에 위치시킵니다.

   <br><img width="60%" src="assets/ffmpeg_2.png" alt="압축 해제">

3. **환경 변수(Path) 설정:**

   - `Win + R`을 누르고 `sysdm.cpl`을 입력하여 시스템 속성 창을 엽니다.

   ![sysdm.cpl 실행](assets/ffmpeg_3.png)

   - **[고급]** 탭 ➔ **[환경 변수]** 버튼을 클릭합니다.
   
   - 시스템 변수 목록에서 `Path`를 찾아 **[편집]**을 누릅니다.

   ![Path 더블 클릭](assets/ffmpeg_4.png)

   - **[새로 만들기]**를 클릭하고 FFmpeg 압축을 해제한 폴더 내의 `bin` 폴더 경로를 추가합니다. (예: `C:\ffmpeg\bin`)

   ![새로 만들기 클릭](assets/ffmpeg_5.png)
   - 확인을 눌러 창을 닫습니다.

4. **설치 확인:**

   명령 프롬프트(CMD)를 열고 `ffmpeg -version`을 입력하여 버전 정보가 정상적으로 출력되는지 확인합니다.

   ![경로 입력 완료](assets/ffmpeg_6.png)

---

### 3. 프로젝트 빌드 및 실행

1. **Qt Creator 실행:** 

   Qt Creator에서 `client/src/metro_smart_subway.pro` 프로젝트 파일을 엽니다.

2. **빌드 설정(Kit):** 

   MinGW 64-bit (Qt 6.x 이상) 키트를 선택합니다.

3. **빌드 및 실행:** 

   좌측 하단의 **실행(Run)** 버튼(▶)을 클릭하거나 `Ctrl+R`을 눌러 클라이언트 애플리케이션을 빌드 및 구동합니다.