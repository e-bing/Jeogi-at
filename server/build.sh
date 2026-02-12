#!/bin/bash
# build.sh

BUILD_DIR="build"

# 1. 기존 빌드 폴더 삭제
if [ "$1" == "clean" ]; then
    echo "Cleaning old build files..."
    rm -rf $BUILD_DIR
fi

# 2. 빌드 폴더 생성
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# 3. CMake 및 Make 실행
echo "Starting build..."
if cmake .. && make -j$(nproc); then
    echo "---------------------------"
    echo "Build Success!"
else
    echo "---------------------------"
    echo "Build Failed!"
    exit 1
fi