import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import com.metro.network 1.0
import ".."

ColumnLayout {
    width: parent.width
    height: parent.height
    spacing: 20

    function getDayName(day) {
        var days = ["", "일요일", "월요일", "화요일", "수요일", "목요일", "금요일", "토요일"];
        return days[day] ? days[day] : day;
    }

    property var realtimeData: []
    property var airStatsData: []
    property var flowStatsData: []

    Timer {
        id: connectionTimer
        interval: 1000
        repeat: false
        onTriggered: {
            console.log("Statistics - Auto-connecting...");
            client.connectToServer(mainWindow.serverIp, mainWindow.serverPort);
        }
    }

    NetworkClient {
        id: client
        onIsConnectedChanged: {
            console.log("Statistics - Connected: " + client.isConnected);
        }
        onStatusMessageChanged: {
            console.log("Statistics - Status: " + client.statusMessage);
        }
        onZoneCongestionReceived: function (zones, totalCount) {
            console.log("Statistics - Zone Congestion Received: " + JSON.stringify(zones));
            var arr = [];
            for (var i = 0; i < zones.length; i++) {
                var count = zones[i];
                var status = "여유";
                var color = "#22C55E";
                if (count >= client.congestionNormalMax) {
                    status = "혼잡";
                    color = "#EF4444";
                } else if (count >= client.congestionEasyMax) {
                    status = "보통";
                    color = "#EAB308";
                }
                arr.push({
                    index: i + 1,
                    count: count,
                    status: status,
                    color: color
                });
            }
            realtimeData = arr;
        }

        onAirStatsReceived: function (data) {
            console.log("Statistics - Air Stats Received: " + data.length);
            var arr = [];
            for (var i = 0; i < data.length; ++i)
                arr.push(data[i]);
            console.log("Air Stats JSON: " + JSON.stringify(arr));
            airStatsData = arr;
        }
        onFlowStatsReceived: function (data) {
            console.log("Statistics - Flow Stats Received: " + data.length);
            var arr = [];
            for (var i = 0; i < data.length; ++i)
                arr.push(data[i]);
            console.log("Flow Stats JSON: " + JSON.stringify(arr));
            flowStatsData = arr;
        }

        Component.onCompleted: {
            console.log("Statistics - Page ready, scheduling connection...");
            connectionTimer.start();
        }
        Component.onDestruction: {
            console.log("Statistics - Disconnecting on destruction...");
            disconnectFromServer();
        }
    }

    // Header: Title + Status
    RowLayout {
        Layout.fillWidth: true
        Layout.margins: 20
        spacing: 20

        Text {
            text: "통계 및 리포트 분석"
            font: Style.fontBold
            color: Style.colorSlate800
        }

        Item {
            Layout.fillWidth: true
        }

        Rectangle {
            width: 12
            height: 12
            radius: 6
            color: client.isConnected ? "#22C55E" : "#EF4444"
        }
        Text {
            text: client.statusMessage
            color: Style.colorSlate500
            font: Style.fontSmall
        }
        Button {
            id: connectBtn
            text: client.isConnected ? "서버 연결 해제" : "서버 연결"
            font.bold: true
            background: Rectangle {
                implicitWidth: 100
                implicitHeight: 32
                color: connectBtn.pressed ? Style.colorPrimaryDark : (connectBtn.hovered ? Style.colorPrimary : "transparent")
                border.color: Style.colorPrimary
                border.width: 1
                radius: 4
            }
            contentItem: Text {
                text: connectBtn.text
                font: connectBtn.font
                color: connectBtn.hovered || connectBtn.pressed ? "white" : Style.colorPrimary
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: {
                if (client.isConnected)
                    client.disconnectFromServer();
                else
                    client.connectToServer(mainWindow.serverIp, mainWindow.serverPort);
            }
        }
    }

    // Tabs
    TabBar {
        id: bar
        Layout.fillWidth: true
        Layout.leftMargin: 20
        Layout.rightMargin: 20
        background: Rectangle {
            color: Style.colorSurface
            border.color: Style.colorTableBorder
            border.width: 1
        }

        TabButton {
            id: tab1
            text: "📊 실시간 혼잡도"
            Layout.fillWidth: true
            background: Rectangle {
                color: tab1.checked ? Style.colorSurface : Style.colorSlate200
                border.color: Style.colorTableBorder
                border.width: 1
            }
            contentItem: Text {
                text: tab1.text
                font: Style.fontRegular
                color: tab1.checked ? Style.colorSlate800 : Style.colorSlate600
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideNone
            }
        }
        TabButton {
            id: tab2
            text: "🌫️ 공기질 통계"
            Layout.fillWidth: true
            background: Rectangle {
                color: tab2.checked ? Style.colorSurface : Style.colorSlate200
                border.color: Style.colorTableBorder
                border.width: 1
            }
            contentItem: Text {
                text: tab2.text
                font: Style.fontRegular
                color: tab2.checked ? Style.colorSlate800 : Style.colorSlate600
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideNone
            }
        }
        TabButton {
            id: tab3
            text: "👥 승객 흐름 통계"
            Layout.fillWidth: true
            background: Rectangle {
                color: tab3.checked ? Style.colorSurface : Style.colorSlate200
                border.color: Style.colorTableBorder
                border.width: 1
            }
            contentItem: Text {
                text: tab3.text
                font: Style.fontRegular
                color: tab3.checked ? Style.colorSlate800 : Style.colorSlate600
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideNone
            }
        }
    }

    StackLayout {
        currentIndex: bar.currentIndex
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: 20
        Layout.rightMargin: 20
        Layout.bottomMargin: 20

        // Tab 1: Realtime Congestion
        Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 30

                Text {
                    text: "👥 실시간 혼잡도 현황 (8구간)"
                    font: Style.fontBold
                    color: Style.colorSlate800
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 15
                    height: 160

                    Repeater {
                        model: 8
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 8
                            color: realtimeData[index] ? realtimeData[index].color : "#22C55E"
                            border.color: "white"
                            border.width: 1

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 10
                                Text {
                                    text: (index + 1)
                                    font.pixelSize: 24
                                    font.bold: true
                                    color: "white"
                                    Layout.alignment: Qt.AlignCenter
                                }
                                Text {
                                    text: (realtimeData[index] ? realtimeData[index].count : 0) + "명"
                                    font.pixelSize: 16
                                    font.bold: true
                                    color: "white"
                                    Layout.alignment: Qt.AlignCenter
                                }
                            }
                        }
                    }
                }

                // Legend
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 20
                    RowLayout {
                        spacing: 8
                        Rectangle {
                            width: 12
                            height: 12
                            radius: 6
                            color: "#22C55E"
                        }
                        Text {
                            text: "여유"
                            color: Style.colorSlate600
                            font: Style.fontSmall
                        }
                    }
                    RowLayout {
                        spacing: 8
                        Rectangle {
                            width: 12
                            height: 12
                            radius: 6
                            color: "#EAB308"
                        }
                        Text {
                            text: "보통"
                            color: Style.colorSlate600
                            font: Style.fontSmall
                        }
                    }
                    RowLayout {
                        spacing: 8
                        Rectangle {
                            width: 12
                            height: 12
                            radius: 6
                            color: "#EF4444"
                        }
                        Text {
                            text: "혼잡"
                            color: Style.colorSlate600
                            font: Style.fontSmall
                        }
                    }
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }

        // Tab 2: Air Quality Stats
        ScrollView {
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: 20

                Text {
                    text: "일주일간의 공기질(CO, 가스) 분석 데이터입니다."
                    color: Style.colorSlate500
                }

                // Group by Day logic is complex in QML directly without a proper model
                // For simplicity, we just list them all or rely on C++ grouping.
                // Assuming flat list for now or we update C++ to send grouped data.
                // Let's just list raw data for V1.
                ListView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 400
                    clip: true
                    model: airStatsData
                    header: RowLayout {
                        width: parent.width
                        spacing: 0
                        Repeater {
                            model: ["요일", "시간", "CO (ppm)\n(임계값: 25)", "CO2 (ppm)\n(임계값: 600)", "상태"]
                            Rectangle {
                                Layout.fillWidth: true
                                height: 40
                                color: Style.colorTableHeadAir
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: "white"
                                    font.bold: true
                                }
                            }
                        }
                    }
                    delegate: RowLayout {
                        width: parent.width
                        height: 40
                        spacing: 0
                        Rectangle {
                            Layout.fillWidth: true
                            height: 40
                            color: Style.colorSurface
                            border.color: Style.colorTableBorder
                            Text {
                                anchors.centerIn: parent
                                text: getDayName(modelData.day)
                                color: Style.colorSlate800
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            height: 40
                            color: Style.colorSurface
                            border.color: Style.colorTableBorder
                            Text {
                                anchors.centerIn: parent
                                text: modelData.hour + "시"
                                color: Style.colorSlate800
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            height: 40
                            color: Style.colorSurface
                            border.color: Style.colorTableBorder
                            Text {
                                anchors.centerIn: parent
                                text: modelData.co.toFixed(2)
                                color: Style.colorSlate800
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            height: 40
                            color: Style.colorSurface
                            border.color: Style.colorTableBorder
                            Text {
                                anchors.centerIn: parent
                                text: modelData.co2 ? modelData.co2.toFixed(2) : "0.00"
                                color: Style.colorSlate800
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            height: 40
                            color: Style.colorSurface
                            border.color: Style.colorTableBorder
                            Text {
                                anchors.centerIn: parent
                                text: (modelData.co_status || "") + " / " + (modelData.gas_status || "")
                                color: Style.colorSlate800
                            }
                        }
                    }
                }
            }
        }

        // Tab 3: Flow Stats
        ScrollView {
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: 20
                ListView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 400
                    clip: true
                    model: flowStatsData
                    header: RowLayout {
                        width: parent.width
                        spacing: 0
                        Repeater {
                            model: ["요일", "시간", "평균 인원", "상태"]
                            Rectangle {
                                Layout.fillWidth: true
                                height: 40
                                color: Style.colorTableHeadFlow
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: "white"
                                    font.bold: true
                                }
                            }
                        }
                    }
                    delegate: RowLayout {
                        width: parent.width
                        height: 40
                        spacing: 0
                        Rectangle {
                            Layout.fillWidth: true
                            height: 40
                            color: Style.colorSurface
                            border.color: Style.colorTableBorder
                            Text {
                                anchors.centerIn: parent
                                text: getDayName(modelData.day)
                                color: Style.colorSlate800
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            height: 40
                            color: Style.colorSurface
                            border.color: Style.colorTableBorder
                            Text {
                                anchors.centerIn: parent
                                text: modelData.hour + "시"
                                color: Style.colorSlate800
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            height: 40
                            color: Style.colorSurface
                            border.color: Style.colorTableBorder
                            Text {
                                anchors.centerIn: parent
                                text: modelData.avg_count + "명"
                                color: Style.colorSlate800
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            height: 40
                            color: Style.colorSurface
                            border.color: Style.colorTableBorder
                            Text {
                                anchors.centerIn: parent
                                text: (modelData.status === "혼잡" ? "🔴 " : (modelData.status === "보통" ? "🟡 " : "🟢 ")) + modelData.status
                                color: Style.colorSlate800
                            }
                        }
                    }
                }
            }
        }
    }
}
