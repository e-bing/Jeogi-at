import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import com.metro.network 1.0
import ".."
import "../components"

Item {
    id: root
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0

    property var systemData: ({
            "server": {
                "cpu_usage": 0,
                "cpu_temp": 0,
                "disk_usage": 0
            },
            "firmware": {
                "cpu_usage": 0,
                "cpu_temp": 0,
                "disk_usage": 0,
                "connected": false
            }
        })

    // ── NetworkClient ──────────────────────────────────────────────────────────
    Timer {
        id: connectionTimer
        interval: 800
        repeat: false
        onTriggered: client.connectToServer(mainWindow.serverIp, mainWindow.serverPort)
    }

    NetworkClient {
        id: client
        onSystemMonitorReceived: function (data) {
            root.systemData = data;
        }
        onIsConnectedChanged: {
            if (!client.isConnected) {
                // 서버 연결 끊기면 펌웨어 포함 모든 데이터 초기화
                var data = {};
                data[client.FIELD_SERVER] = {};
                data[client.FIELD_SERVER][client.FIELD_CPU_USAGE] = 0;
                data[client.FIELD_SERVER][client.FIELD_CPU_TEMP] = 0;
                data[client.FIELD_SERVER][client.FIELD_DISK_USAGE] = 0;

                data[client.FIELD_FIRMWARE] = {};
                data[client.FIELD_FIRMWARE][client.FIELD_CPU_USAGE] = 0;
                data[client.FIELD_FIRMWARE][client.FIELD_CPU_TEMP] = 0;
                data[client.FIELD_FIRMWARE][client.FIELD_DISK_USAGE] = 0;
                data[client.FIELD_FIRMWARE][client.FIELD_CONNECTED] = false;
                root.systemData = data;
            }
        }
        Component.onCompleted: connectionTimer.start()
        Component.onDestruction: disconnectFromServer()
    }

    // ── Layout ─────────────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 20

        // Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                spacing: 4
                Text {
                    text: "🖥️  시스템 리소스 모니터링"
                    font.family: "Pretendard"
                    font.pixelSize: 22
                    font.bold: true
                    color: Style.colorSlate800
                }
                Text {
                    text: "서버 및 펌웨어의 실시간 하드웨어 상태를 확인합니다."
                    font.family: "Pretendard"
                    font.pixelSize: 12
                    color: Style.colorSlate500
                }
            }

            Item {
                Layout.fillWidth: true
            }

            // Connection dot
            Rectangle {
                width: 10
                height: 10
                radius: 5
                color: client.isConnected ? "#22C55E" : "#EF4444"
            }
            Text {
                text: client.statusMessage
                font.family: "Pretendard"
                font.pixelSize: 12
                color: Style.colorSlate500
            }

            // Connect / Disconnect button
            Rectangle {
                id: connBtn
                width: 90
                height: 32
                radius: 8
                color: connBtnArea.containsPress ? Style.colorSlate300 : connBtnArea.containsMouse ? Style.colorSlate200 : "transparent"
                border.color: Style.colorSlate300
                border.width: 1
                Text {
                    anchors.centerIn: parent
                    text: client.isConnected ? "연결 해제" : "서버 연결"
                    font.family: "Pretendard"
                    font.pixelSize: 12
                    color: Style.colorSlate700 !== undefined ? Style.colorSlate700 : Style.colorSlate600
                }
                MouseArea {
                    id: connBtnArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: client.isConnected ? client.disconnectFromServer() : client.connectToServer(mainWindow.serverIp, mainWindow.serverPort)
                }
            }
        }

        // Divider
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Style.colorSlate200
        }

        // Cards row
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 20

            // ── Server Card ───────────────────────────────────────────────────
            StatCard {
                Layout.fillWidth: true
                Layout.fillHeight: true

                iconEmoji: "☁️"
                iconBg: client.isConnected ? "#E0F2FE" : "#F1F5F9"
                cardTitle: "중앙 관리 서버 (Server)"
                cardSub: "CentOS 7 · Intel Xeon" + (mainWindow.serverIp.length > 0 ? " · " + mainWindow.serverIp : "")
                isOnline: client.isConnected
                opacity: client.isConnected ? 1.0 : 0.55
                disconnectMsg: "서버 연결 없음"

                stat1Title: "CPU 점유율"
                stat1Value: root.systemData[client.FIELD_SERVER][client.FIELD_CPU_USAGE]
                stat1Unit: "%"
                stat1Color: "#0EA5E9"

                stat2Title: "CPU 온도"
                stat2Value: root.systemData[client.FIELD_SERVER][client.FIELD_CPU_TEMP]
                stat2Unit: "°C"
                stat2Color: "#F43F5E"

                stat3Title: "디스크 사용량"
                stat3Value: root.systemData[client.FIELD_SERVER][client.FIELD_DISK_USAGE]
                stat3Unit: "%"
                stat3Color: "#8B5CF6"
            }

            // ── Firmware Card ─────────────────────────────────────────────────
            StatCard {
                Layout.fillWidth: true
                Layout.fillHeight: true

                iconEmoji: "📟"
                iconBg: root.systemData[client.FIELD_FIRMWARE][client.FIELD_CONNECTED] ? "#DCFCE7" : "#FEE2E2"
                cardTitle: "에지 디바이스 (Firmware)"
                cardSub: root.systemData[client.FIELD_FIRMWARE][client.FIELD_CONNECTED] ? "연결됨 · Raspberry Pi 4" : "연결 끊김"
                isOnline: root.systemData[client.FIELD_FIRMWARE][client.FIELD_CONNECTED]
                opacity: root.systemData[client.FIELD_FIRMWARE][client.FIELD_CONNECTED] ? 1.0 : 0.55

                stat1Title: "CPU 점유율"
                stat1Value: root.systemData[client.FIELD_FIRMWARE][client.FIELD_CPU_USAGE]
                stat1Unit: "%"
                stat1Color: "#22C55E"

                stat2Title: "CPU 온도"
                stat2Value: root.systemData[client.FIELD_FIRMWARE][client.FIELD_CPU_TEMP]
                stat2Unit: "°C"
                stat2Color: "#F97316"

                stat3Title: "디스크 사용량"
                stat3Value: root.systemData[client.FIELD_FIRMWARE][client.FIELD_DISK_USAGE]
                stat3Unit: "%"
                stat3Color: "#6366F1"

                disconnectMsg: "펌웨어 연결 없음"
            }
        }
    }

    // ── Inline component: StatCard ─────────────────────────────────────────────
    component StatCard: Rectangle {
        color: Style.colorSurface
        radius: 18
        border.color: Style.colorSlate200
        border.width: 1

        property string iconEmoji: "🖥️"
        property color iconBg: "#E0F2FE"
        property string cardTitle: ""
        property string cardSub: ""
        property bool isOnline: true
        property string disconnectMsg: ""

        property string stat1Title: ""
        property real stat1Value: 0
        property string stat1Unit: ""
        property color stat1Color: "#0EA5E9"
        property string stat2Title: ""
        property real stat2Value: 0
        property string stat2Unit: ""
        property color stat2Color: "#F43F5E"
        property string stat3Title: ""
        property real stat3Value: 0
        property string stat3Unit: ""
        property color stat3Color: "#8B5CF6"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 28
            spacing: 22

            // Card header
            RowLayout {
                Layout.fillWidth: true
                spacing: 14

                Rectangle {
                    width: 46
                    height: 46
                    radius: 12
                    color: iconBg
                    Text {
                        anchors.centerIn: parent
                        text: iconEmoji
                        font.pixelSize: 22
                    }
                }

                ColumnLayout {
                    spacing: 3
                    Text {
                        text: cardTitle
                        font.family: "Pretendard"
                        font.pixelSize: 16
                        font.bold: true
                        color: Style.colorSlate800
                    }
                    RowLayout {
                        spacing: 6
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: isOnline ? "#22C55E" : "#94A3B8"
                        }
                        Text {
                            text: isOnline ? "연결됨" : "연결 끊김"
                            font.family: "Pretendard"
                            font.pixelSize: 11
                            font.bold: true
                            color: isOnline ? "#16A34A" : Style.colorSlate500
                        }
                    }
                }
            }

            // Divider
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Style.colorSlate200
            }

            // Stats (shown when connected or always for server)
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 18
                visible: isOnline || disconnectMsg === ""

                Repeater {
                    model: [
                        {
                            t: stat1Title,
                            v: stat1Value,
                            u: stat1Unit,
                            c: stat1Color
                        },
                        {
                            t: stat2Title,
                            v: stat2Value,
                            u: stat2Unit,
                            c: stat2Color
                        },
                        {
                            t: stat3Title,
                            v: stat3Value,
                            u: stat3Unit,
                            c: stat3Color
                        }
                    ]
                    delegate: ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: modelData.t
                                font.family: "Pretendard"
                                font.pixelSize: 12
                                font.bold: true
                                color: Style.colorSlate600
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                            Text {
                                text: modelData.v.toFixed(1) + modelData.u
                                font.family: "Pretendard"
                                font.pixelSize: 13
                                font.bold: true
                                color: Style.colorSlate800
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 9
                            radius: 5
                            color: Style.isDarkMode ? "#334155" : "#F1F5F9"
                            clip: true

                            Rectangle {
                                height: parent.height
                                radius: 5
                                width: Math.min(parent.width, (modelData.v / 100.0) * parent.width)
                                color: modelData.c
                                Behavior on width {
                                    NumberAnimation {
                                        duration: 900
                                        easing.type: Easing.OutCubic
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Disconnected placeholder
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8
                visible: !isOnline && disconnectMsg !== ""

                Item {
                    Layout.fillHeight: true
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: disconnectMsg
                    horizontalAlignment: Text.AlignHCenter
                    font.family: "Pretendard"
                    font.pixelSize: 13
                    color: Style.colorSlate500
                    lineHeight: 1.5
                }
                Item {
                    Layout.fillHeight: true
                }
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }
}
