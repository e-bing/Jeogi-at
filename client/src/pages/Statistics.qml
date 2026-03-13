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

    property var airStatsData: []
    property var flowStatsData: []
    property var tempHumiStatsData: []

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
            var groups = {};
            for (var i = 0; i < data.length; ++i) {
                var item = data[i];
                var key = item.day + "_" + item.hour;
                if (!groups[key]) {
                    groups[key] = {
                        day: item.day,
                        hour: item.hour,
                        platforms: [0, 0, 0, 0, 0, 0, 0, 0],
                        total: 0
                    };
                }
                var pIdx = item.platform - 1;
                if (pIdx >= 0 && pIdx < 8) {
                    groups[key].platforms[pIdx] = item.avg_count;
                    groups[key].total += item.avg_count;
                }
            }
            var arr = [];
            for (var k in groups) {
                arr.push(groups[k]);
            }
            flowStatsData = arr;
        }
        onTempHumiStatsReceived: function (data) {
            console.log("Statistics - Temp/Humi Stats Received: " + data.length);
            var arr = [];
            for (var i = 0; i < data.length; ++i)
                arr.push(data[i]);
            tempHumiStatsData = arr;
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
            id: tab4
            text: "🌡️ 온습도 통계"
            Layout.fillWidth: true
            background: Rectangle {
                color: tab4.checked ? Style.colorSurface : Style.colorSlate200
                border.color: Style.colorTableBorder
                border.width: 1
            }
            contentItem: Text {
                text: tab4.text
                font: Style.fontRegular
                color: tab4.checked ? Style.colorSlate800 : Style.colorSlate600
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



        // Tab 2: Air Quality Stats
        ScrollView {
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: 20

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
                            model: ["요일", "시간", "CO (ppm)", "CO2 (ppm)", "상태"]
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

        // Tab 3: Temp/Humi Stats
        ScrollView {
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: 20
                ListView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 400
                    clip: true
                    model: tempHumiStatsData
                    header: RowLayout {
                        width: parent.width
                        spacing: 0
                        Repeater {
                            model: ["요일", "시간", "온도 (°C)", "습도 (%)"]
                            Rectangle {
                                Layout.fillWidth: true
                                height: 40
                                color: "#3B82F6"
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
                                text: modelData.temp.toFixed(1) + "°C"
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
                                text: modelData.humi.toFixed(1) + "%"
                                color: Style.colorSlate800
                            }
                        }
                    }
                }
            }
        }

        // Tab 4: Flow Stats
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
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 40
                            height: 40
                            color: Style.colorTableHeadFlow
                            Text {
                                anchors.centerIn: parent
                                text: "요일"
                                color: "white"
                                font.bold: true
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 40
                            height: 40
                            color: Style.colorTableHeadFlow
                            Text {
                                anchors.centerIn: parent
                                text: "시간"
                                color: "white"
                                font.bold: true
                            }
                        }
                        Repeater {
                            model: 8
                            Rectangle {
                                Layout.preferredWidth: 50
                                height: 40
                                color: Style.colorTableHeadFlow
                                Text {
                                    anchors.centerIn: parent
                                    text: (index + 1)
                                    color: "white"
                                    font.bold: true
                                }
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 60
                            height: 40
                            color: Style.colorTableHeadFlow
                            Text {
                                anchors.centerIn: parent
                                text: "전체 평균"
                                color: "white"
                                font.bold: true
                            }
                        }
                    }
                    delegate: RowLayout {
                        width: parent.width
                        height: 40
                        spacing: 0
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 40
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
                            Layout.preferredWidth: 40
                            height: 40
                            color: Style.colorSurface
                            border.color: Style.colorTableBorder
                            Text {
                                anchors.centerIn: parent
                                text: modelData.hour + "시"
                                color: Style.colorSlate800
                            }
                        }
                        Repeater {
                            model: 8
                            Rectangle {
                                Layout.preferredWidth: 50
                                height: 40
                                color: Style.colorSurface
                                border.color: Style.colorTableBorder
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    radius: 2
                                    color: getCongestionColor(modelData.platforms[index])
                                    opacity: 0.9
                                    Text {
                                        anchors.centerIn: parent
                                        text: Math.round(modelData.platforms[index])
                                        color: "white"
                                        font.bold: true
                                    }
                                }
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 60
                            height: 40
                            color: Style.colorSurface
                            border.color: Style.colorTableBorder
                            Text {
                                anchors.centerIn: parent
                                text: Math.round(modelData.total / 8) + "명"
                                color: Style.colorSlate800
                                font.bold: true
                            }
                        }
                    }
                }
            }
        }
    }
}
