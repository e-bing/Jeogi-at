cmake_minimum_required(VERSION 3.10)
project(cctv_server)

set(CMAKE_CXX_STANDARD 17)

# 1. 패키지 탐색
find_package(OpenCV REQUIRED)
find_package(SDL2 REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)

find_library(PAHO_CPP_LIB paho-mqttpp3 REQUIRED)
find_library(PAHO_C_LIB paho-mqtt3as REQUIRED)
find_path(PAHO_INCLUDE_DIR mqtt/async_client.h REQUIRED)

# FFmpeg 및 MariaDB 클라이언트 라이브러리 추가
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(SWSCALE REQUIRED libswscale)
pkg_check_modules(MARIADB REQUIRED libmariadb) # DB 저장용

# 2. 헤더 경로 포함
include_directories(
    include
    includes
    ${PAHO_INCLUDE_DIR}    
    ${AVCODEC_INCLUDE_DIRS}
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${SWSCALE_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${MARIADB_INCLUDE_DIRS}
)

# 3. 통합 실행 파일 생성
add_executable(server 
    src/main.cpp 
    src/hanwha_node.cpp 
    src/pi_node.cpp 
    src/shared_data.cpp
    src/database.cpp
    src/sensor.cpp
    src/qt.cpp
    src/motor.cpp
    src/parser.cpp
)

# 3-1. 설정용 실행 파일 생성
add_executable(setup src/setup.cpp)

# 4. 라이브러리 링크
target_link_libraries(server
    ${OpenCV_LIBS}
    ${PAHO_CPP_LIB}      # ✅ PahoMqttCpp::paho-mqttpp3 대신 직접 찾은 경로 사용
    ${PAHO_C_LIB}        # ✅ 의존성인 C 라이브러리도 함께 링크 
    ${SDL2_LIBRARIES}
    ${AVCODEC_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWSCALE_LIBRARIES}
    ${MARIADB_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    pthread
    tinyxml2
)

target_link_libraries(setup PRIVATE nlohmann_json::nlohmann_json)
