QT += core gui quick network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# 공유 프로토콜 헤더 (Qt ↔ 서버 공통)
INCLUDEPATH += ../../../protocol

win32 {
    OPENSSL_DIR = "C:/Qt/Tools/mingw1310_64/opt"
    INCLUDEPATH += $$OPENSSL_DIR/include
    LIBS += -L$$OPENSSL_DIR/lib -lssl -lcrypto
}

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    backend/networkclient.cpp

HEADERS += \
    backend/networkclient.h

RESOURCES += qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
