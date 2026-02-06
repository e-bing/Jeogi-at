cmake_minimum_required(VERSION 3.16)

project(jeogi VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Qt 컴포넌트 추가 (Widgets, Network)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

# 2. OpenSSL 수동 설정 (find_package 대신 직접 설정)
if(WIN32)
    set(OPENSSL_ROOT_DIR "C:/OpenSSL-Win32")
    set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include")

    # 라이브러리 파일 찾기 (여러 이름 시도)
    find_library(OPENSSL_SSL_LIBRARY
        NAMES ssl libssl ssleay32
        PATHS "${OPENSSL_ROOT_DIR}/lib" "${OPENSSL_ROOT_DIR}/lib/VC/x64/MD"
        NO_DEFAULT_PATH
    )

    find_library(OPENSSL_CRYPTO_LIBRARY
        NAMES crypto libcrypto libeay32
        PATHS "${OPENSSL_ROOT_DIR}/lib" "${OPENSSL_ROOT_DIR}/lib/VC/x64/MD"
        NO_DEFAULT_PATH
    )

    if(OPENSSL_SSL_LIBRARY AND OPENSSL_CRYPTO_LIBRARY)
        set(OPENSSL_FOUND TRUE)
        set(OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
        message(STATUS "✅ OpenSSL 찾음!")
        message(STATUS "   Include: ${OPENSSL_INCLUDE_DIR}")
        message(STATUS "   SSL Lib: ${OPENSSL_SSL_LIBRARY}")
        message(STATUS "   Crypto Lib: ${OPENSSL_CRYPTO_LIBRARY}")
    else()
        message(WARNING "⚠️ OpenSSL 라이브러리를 찾을 수 없습니다")
        message(STATUS "   찾은 SSL: ${OPENSSL_SSL_LIBRARY}")
        message(STATUS "   찾은 Crypto: ${OPENSSL_CRYPTO_LIBRARY}")
    endif()
endif()

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(jeogi
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(jeogi SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(jeogi
            ${PROJECT_SOURCES}
        )
    endif()
endif()

# 3. 인클루드 디렉토리 추가
if(OPENSSL_FOUND)
    target_include_directories(jeogi PRIVATE ${OPENSSL_INCLUDE_DIR})
endif()

# 4. 라이브러리 링크
target_link_libraries(jeogi PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

if(OPENSSL_FOUND)
    target_link_libraries(jeogi PRIVATE ${OPENSSL_LIBRARIES})

    # DLL 파일을 실행 파일과 같은 디렉토리로 복사 (런타임 에러 방지)
    if(WIN32)
        add_custom_command(TARGET jeogi POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${OPENSSL_ROOT_DIR}/bin/libssl-3-x64.dll"
                $<TARGET_FILE_DIR:jeogi>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${OPENSSL_ROOT_DIR}/bin/libcrypto-3-x64.dll"
                $<TARGET_FILE_DIR:jeogi>
            COMMENT "OpenSSL DLL 복사 중..."
        )
    endif()
endif()

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.jeogi)
endif()

set_target_properties(jeogi PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS jeogi
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(jeogi)
endif()
