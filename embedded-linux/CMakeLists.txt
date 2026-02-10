cmake_minimum_required(VERSION 3.10)
project(camera_node)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenCV REQUIRED)
find_package(PahoMqttCpp REQUIRED)

# NCNN 찾기
set(ncnn_DIR "/home/iam/ncnn/build/install/lib/cmake/ncnn")
find_package(ncnn REQUIRED)

add_executable(camera_node src/main.cpp)

# 라이브러리 링크 수정
target_link_libraries(camera_node 
    ${OpenCV_LIBS} 
    ncnn
    paho-mqttpp3 
    paho-mqtt3as 
)