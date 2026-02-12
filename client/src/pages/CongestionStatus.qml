import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import com.metro.network 1.0
import ".."

ScrollView {
    clip: true
    contentWidth: availableWidth

    property var congestionData: []
    property var sectionAverages: []
    property var sectionSums: []
    property int grandTotalOccupants: 0

    Timer {
        id: connectionTimer
        interval: 1000
        repeat: false
        onTriggered: {
            console.log("Congestion Status - Auto-connecting...");
            client.connectToServer("aboy.local", 12345);
        }
    }

    // Function to calculate section sums and averages from realtime data
    function calculateSectionData(realtimeData) {
        var sections = [0, 0, 0, 0, 0, 0, 0, 0];
        var counts = [0, 0, 0, 0, 0, 0, 0, 0];
        var total = 0;

        for (var i = 0; i < realtimeData.length; i++) {
            var item = realtimeData[i];
            var platform = item.platform;

            // Extract section number (e.g., "1-1" → section 0)
            if (typeof platform === "string") {
                var parts = platform.split("-");
                if (parts.length === 2) {
                    var mainSection = parseInt(parts[0]) - 1; // Convert to 0-indexed
                    if (mainSection >= 0 && mainSection < 8) {
                        sections[mainSection] += item.count;
                        counts[mainSection]++;
                        total += item.count;
                    }
                }
            }
        }

        // Calculate averages and sums
        var averages = [];
        var sums = [];
        for (var j = 0; j < 8; j++) {
            sums.push(sections[j]);
            averages.push(counts[j] > 0 ? sections[j] / counts[j] : 0);
        }

        return {
            averages: averages,
            sums: sums,
            total: total
        };
    }

    // Function to get color based on congestion level (sum-based)
    function getCongestionColor(sumCount) {
        if (sumCount < 250)
            return "#22C55E";
        else
        // Green
        if (sumCount < 400)
            return "#EAB308";
        else
            // Yellow
            return "#EF4444"; // Red
    }

    // Function to get status text
    function getCongestionStatus(avgCount) {
        if (avgCount < 50) {
            return "여유";
        } else if (avgCount < 100) {
            return "보통";
        } else {
            return "혼잡";
        }
    }

    NetworkClient {
        id: client
        onIsConnectedChanged: {
            console.log("Congestion Status - Connected: " + client.isConnected);
        }
        onRealtimeDataReceived: function (data) {
            console.log("Congestion Status - Realtime Data Received: " + data.length);
            congestionData = data;
            var results = calculateSectionData(data);
            sectionAverages = results.averages;
            sectionSums = results.sums;
            grandTotalOccupants = results.total;
            congestionRepeater.model = sectionAverages.length;
        }

        Component.onCompleted: {
            console.log("Congestion Status - Page ready, scheduling connection...");
            connectionTimer.start();
        }
        Component.onDestruction: {
            console.log("Congestion Status - Disconnecting on destruction...");
            disconnectFromServer();
        }
    }

    ColumnLayout {
        width: parent.width
        spacing: 20

        // Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 80
            Layout.margins: 20
            color: Style.colorSurface
            radius: 12
            border.color: Style.colorSlate200
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 20

                Text {
                    text: "📊 실시간 혼잡도 현황 (REAL-TIME TIMELINE)"
                    font {
                        family: Style.fontBold.family
                        bold: true
                        pixelSize: 18
                    }
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
            }
        }

        // Main Timeline Card
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 300
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            color: Style.colorSurface
            radius: 12
            border.color: Style.colorSlate200
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 30
                spacing: 20

                Text {
                    text: "구역별 디바이스 통합 상태 (8구간)"
                    font: Style.fontBold
                    color: Style.colorSlate800
                }

                // Timeline Visualization
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 80
                    spacing: 4

                    Repeater {
                        id: congestionRepeater
                        model: 8

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 6
                            color: {
                                if (sectionSums.length > index) {
                                    return getCongestionColor(sectionSums[index]);
                                }
                                return Style.colorSlate200;
                            }

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 4

                                Text {
                                    text: (index + 1).toString()
                                    font.pixelSize: 20
                                    font.bold: true
                                    color: "white"
                                    Layout.alignment: Qt.AlignHCenter
                                }

                                Text {
                                    text: {
                                        if (sectionSums.length > index) {
                                            return Math.round(sectionSums[index]) + "명";
                                        }
                                        return "0명";
                                    }
                                    font.pixelSize: 11
                                    color: "white"
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }

                            // Hover effect
                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                onEntered: parent.opacity = 0.8
                                onExited: parent.opacity = 1.0
                            }
                        }
                    }
                }

                // Legend
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 30

                    RowLayout {
                        spacing: 8
                        Rectangle {
                            width: 12
                            height: 12
                            radius: 6
                            color: "#22C55E"
                        }
                        Text {
                            text: "여유 (Normal)"
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
                            text: "보통 (Caution)"
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
                            text: "혼잡 (Critical)"
                            color: Style.colorSlate600
                            font: Style.fontSmall
                        }
                    }

                    Item {
                        width: 20
                        height: 1
                    }

                    Text {
                        text: "전체 실시간 인원: <font color='#F97316'>" + grandTotalOccupants + "명</font>"
                        font.bold: true
                        color: Style.colorSlate800
                        textFormat: Text.RichText
                    }
                }
            }
        }

        // Detailed Section Breakdown
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 400
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.bottomMargin: 20
            color: Style.colorSurface
            radius: 12
            border.color: Style.colorSlate200
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 15

                Text {
                    text: "구역별 상세 정보"
                    font: Style.fontBold
                    color: Style.colorSlate800
                }

                // Headers
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Repeater {
                        model: ["구역", "평균 인원", "상태", "포함 플랫폼"]
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40
                            color: Style.colorPrimary

                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                color: "white"
                                font.bold: true
                                font.pixelSize: 13
                            }
                        }
                    }
                }

                // Data rows
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: 8
                    spacing: 0

                    delegate: RowLayout {
                        width: ListView.view.width
                        height: 50
                        spacing: 0

                        property real avgCount: sectionAverages.length > index ? sectionAverages[index] : 0

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: index % 2 === 0 ? Style.colorSurface : Style.colorBackground
                            border.color: Style.colorTableBorder

                            Text {
                                anchors.centerIn: parent
                                text: "구역 " + (index + 1)
                                font.bold: true
                                color: Style.colorSlate800
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: index % 2 === 0 ? Style.colorSurface : Style.colorBackground
                            border.color: Style.colorTableBorder

                            Text {
                                anchors.centerIn: parent
                                text: Math.round(avgCount) + "명"
                                color: Style.colorSlate600
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: index % 2 === 0 ? Style.colorSurface : Style.colorBackground
                            border.color: Style.colorTableBorder

                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 8

                                Rectangle {
                                    width: 10
                                    height: 10
                                    radius: 5
                                    color: getCongestionColor(avgCount)
                                }

                                Text {
                                    text: getCongestionStatus(avgCount)
                                    color: Style.colorSlate600
                                    font.bold: true
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: index % 2 === 0 ? Style.colorSurface : Style.colorBackground
                            border.color: Style.colorTableBorder

                            Text {
                                anchors.centerIn: parent
                                text: (index + 1) + "-1, " + (index + 1) + "-2, " + (index + 1) + "-3, " + (index + 1) + "-4"
                                color: Style.colorSlate500
                                font.pixelSize: 11
                            }
                        }
                    }
                }
            }
        }
    }
}
