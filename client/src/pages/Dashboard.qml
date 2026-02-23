import QtQuick 2.15
import QtQuick.Shapes 1.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import com.metro.network 1.0
import ".."

ColumnLayout {
    id: dashboardRoot
    width: parent.width
    height: parent.height
    spacing: 0

    property var realtimeData: []
    property var airStatsData: ({})
    property var sectionAverages: []
    property var sectionSums: []
    property int grandTotalOccupants: 0
    property bool isManualMode: false

    Timer {
        id: connectionTimer
        interval: 1000
        repeat: false
        onTriggered: {
            console.log("Dashboard - Auto-connecting...");
            client.connectToServer(mainWindow.serverIp, mainWindow.serverPort);
        }
    }

    // Process data to calculate section sums and averages
    function processSectionData(data) {
        var sections = [0, 0, 0, 0, 0, 0, 0, 0];
        var counts = [0, 0, 0, 0, 0, 0, 0, 0];
        var total = 0;

        for (var i = 0; i < data.length; i++) {
            var item = data[i];
            var platform = item.platform;

            if (typeof platform === "string") {
                var parts = platform.split("-");
                if (parts.length === 2) {
                    var mainSection = parseInt(parts[0]) - 1;
                    if (mainSection >= 0 && mainSection < 8) {
                        sections[mainSection] += item.count;
                        counts[mainSection]++;
                        total += item.count;
                    }
                }
            }
        }

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

    function getCongestionColor(sumCount) {
        if (sumCount < 250)
            return "#22C55E";
        else if (sumCount < 400)
            return "#EAB308";
        else
            return "#EF4444";
    }

    NetworkClient {
        id: client
        onIsConnectedChanged: {
            console.log("Dashboard - Connected: " + client.isConnected);
        }
        onStatusMessageChanged: {
            console.log("Dashboard - Status: " + client.statusMessage);
        }
        onRealtimeDataReceived: function (data) {
            console.log("Dashboard - Realtime Data Received: " + data.length);
            realtimeData = data;
            var results = processSectionData(data);
            sectionAverages = results.averages;
            sectionSums = results.sums;
            grandTotalOccupants = results.total;
        }
        onAirStatsReceived: function (data) {
            console.log("Dashboard - Historical Air Stats Received: " + data.length);
        }
        onRealtimeAirReceived: function (data) {
            console.log("Dashboard - Real-time Air Data Received: " + JSON.stringify(data));
            dashboardRoot.airStatsData = data;
        }
        Component.onCompleted: {
            console.log("Dashboard - Page ready, scheduling connection...");
            connectionTimer.start();
        }
        Component.onDestruction: {
            console.log("Dashboard - Disconnecting on destruction...");
            disconnectFromServer();
        }
    }

    // Header: Title + Status (Added for consistency and feedback)
    RowLayout {
        Layout.fillWidth: true
        Layout.margins: 20
        spacing: 20

        Text {
            text: "실시간 혼잡도 및 환경 모니터링"
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
            text: client.isConnected ? "Disconnect" : "Connect"
            onClicked: {
                if (client.isConnected)
                    client.disconnectFromServer();
                else
                    client.connectToServer(mainWindow.serverIp, mainWindow.serverPort);
            }
        }
    }

    ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 20

            // Top Row: Cameras + Environment
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 400
                spacing: 20

                // Camera Card
                Rectangle {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.preferredWidth: 6
                    color: Style.colorSurface
                    radius: 12
                    border.color: Style.colorSlate200
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 15

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "📹 실시간 카메라 모니터링"
                                font: Style.fontBold
                                color: Style.colorSlate800
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                            Text {
                                id: detailsText
                                text: "상세보기"
                                color: Style.colorPrimary
                                font.pixelSize: 12
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        console.log("상세보기 클릭됨");
                                    }
                                }
                            }
                        }

                        GridLayout {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: 10
                            columnSpacing: 10

                            Repeater {
                                model: 4
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: Style.isDarkMode ? "#FFFFFF" : "#1E293B"
                                    radius: 8
                                    clip: true
                                    border.color: Style.isDarkMode ? Style.colorSlate300 : "transparent"
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: index === 2 ? "NO SIGNAL" : "CAM-0" + (index + 1)
                                        color: index === 2 ? Style.colorDanger : (Style.isDarkMode ? "#0F172A" : "white")
                                        font.bold: true
                                    }

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.margins: 10
                                        width: 60
                                        height: 20
                                        radius: 4
                                        color: index === 2 ? Style.colorDanger : "#22C55E"

                                        RowLayout {
                                            anchors.centerIn: parent
                                            spacing: 4
                                            Rectangle {
                                                width: 6
                                                height: 6
                                                radius: 3
                                                color: "white"
                                            }
                                            Text {
                                                text: "LIVE"
                                                color: "white"
                                                font.pixelSize: 10
                                                font.bold: true
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Environment Card
                Rectangle {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.preferredWidth: 4
                    color: Style.colorSurface
                    radius: 12
                    border.color: Style.colorSlate200
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 20

                        Text {
                            text: "🍂 역사 내 환경 모니터링"
                            font: Style.fontBold
                            color: Style.colorSlate800
                        }

                        // CO2
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            RowLayout {
                                Text {
                                    text: "이산화탄소 농도 (CO2)"
                                    color: Style.colorSlate500
                                    font: Style.fontSmall
                                }
                                Item {
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: dashboardRoot.airStatsData && dashboardRoot.airStatsData.co2_ppm ? dashboardRoot.airStatsData.co2_ppm.toFixed(1) : "0"
                                    font: Style.fontLarge
                                    color: Style.colorSlate800
                                }
                                Text {
                                    text: "ppm"
                                    color: Style.colorSlate500
                                    font: Style.fontSmall
                                    Layout.alignment: Qt.AlignBaseline
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                height: 8
                                radius: 4
                                color: Style.colorSlate200
                                clip: true

                                Rectangle {
                                    height: parent.height
                                    radius: 4
                                    width: Math.min(parent.width, (dashboardRoot.airStatsData && dashboardRoot.airStatsData.co2_ppm ? (dashboardRoot.airStatsData.co2_ppm / 1200) * parent.width : 0))
                                    color: {
                                        if (!dashboardRoot.airStatsData || !dashboardRoot.airStatsData.gas_status)
                                            return Style.colorSlate200;
                                        var s = dashboardRoot.airStatsData.gas_status;
                                        if (s.indexOf("정상") !== -1 || s.indexOf("양호") !== -1)
                                            return "#22C55E";
                                        if (s.indexOf("주의") !== -1 || s.indexOf("보통") !== -1)
                                            return "#EAB308";
                                        if (s.indexOf("위험") !== -1 || s.indexOf("나쁨") !== -1)
                                            return "#EF4444";
                                        return Style.colorSlate200;
                                    }
                                    Behavior on width {
                                        NumberAnimation {
                                            duration: 800
                                            easing.type: Easing.OutCubic
                                        }
                                    }
                                }
                            }
                        }

                        // AQI
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            RowLayout {
                                Text {
                                    text: "일산화탄소 농도 (CO)"
                                    color: Style.colorSlate500
                                    font: Style.fontSmall
                                }
                                Item {
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: dashboardRoot.airStatsData && dashboardRoot.airStatsData.co_level ? dashboardRoot.airStatsData.co_level.toFixed(1) : "0"
                                    font: Style.fontLarge
                                    color: Style.colorSlate800
                                }
                                Text {
                                    text: "ppm"
                                    color: Style.colorSlate500
                                    font: Style.fontSmall
                                    Layout.alignment: Qt.AlignBaseline
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                height: 8
                                radius: 4
                                color: Style.colorSlate200
                                clip: true

                                Rectangle {
                                    height: parent.height
                                    radius: 4
                                    width: Math.min(parent.width, (dashboardRoot.airStatsData && dashboardRoot.airStatsData.co_level ? (dashboardRoot.airStatsData.co_level / 50) * parent.width : 0))
                                    color: {
                                        if (!dashboardRoot.airStatsData || !dashboardRoot.airStatsData.co_status)
                                            return Style.colorSlate200;
                                        var s = dashboardRoot.airStatsData.co_status;
                                        if (s.indexOf("정상") !== -1 || s.indexOf("양호") !== -1)
                                            return "#22C55E";
                                        if (s.indexOf("주의") !== -1 || s.indexOf("보통") !== -1)
                                            return "#EAB308";
                                        if (s.indexOf("위험") !== -1 || s.indexOf("나쁨") !== -1)
                                            return "#EF4444";
                                        return Style.colorSlate200;
                                    }
                                    Behavior on width {
                                        NumberAnimation {
                                            duration: 800
                                            easing.type: Easing.OutCubic
                                        }
                                    }
                                }
                            }
                        }

                        // Alert Box
                        Rectangle {
                            id: alertBox
                            Layout.fillWidth: true
                            Layout.minimumHeight: 60
                            property int alertLevel: {
                                if (!dashboardRoot.airStatsData)
                                    return 0;
                                var sCo = dashboardRoot.airStatsData.co_status || "";
                                var sGas = dashboardRoot.airStatsData.gas_status || "";

                                var isDanger = (sCo.indexOf("위험") !== -1 || sCo.indexOf("나쁨") !== -1 || sGas.indexOf("위험") !== -1 || sGas.indexOf("나쁨") !== -1);
                                if (isDanger)
                                    return 2;

                                var isCaution = (sCo.indexOf("보통") !== -1 || sCo.indexOf("주의") !== -1 || sGas.indexOf("보통") !== -1 || sGas.indexOf("주의") !== -1);
                                if (isCaution)
                                    return 1;

                                return 0;
                            }
                            color: alertLevel === 2 ? Style.colorRedAlert : (alertLevel === 1 ? Style.colorYellowAlert : Style.colorGreenAlert)
                            radius: 8
                            border.color: alertLevel === 2 ? Style.colorRedAlertBorder : (alertLevel === 1 ? Style.colorYellowAlertBorder : Style.colorGreenAlertBorder)

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 12

                                Rectangle {
                                    width: 32
                                    height: 32
                                    radius: 16
                                    Layout.alignment: Qt.AlignVCenter
                                    color: {
                                        var lvl = alertBox.alertLevel;
                                        if (lvl === 2)
                                            return Style.colorRedAlertBorder;
                                        if (lvl === 1)
                                            return Style.colorYellowAlertBorder;
                                        return Style.colorGreenAlertBorder;
                                    }
                                    Text {
                                        anchors.centerIn: parent
                                        text: {
                                            var lvl = alertBox.alertLevel;
                                            if (lvl === 2)
                                                return "🔴";
                                            if (lvl === 1)
                                                return "🟡";
                                            return "🟢";
                                        }
                                        font.pixelSize: 16
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text {
                                        Layout.fillWidth: true
                                        text: {
                                            var lvl = dashboardRoot.airStatsData ? alertBox.alertLevel : 0;
                                            if (lvl === 2)
                                                return "위험 단계";
                                            if (lvl === 1)
                                                return "주의 단계";
                                            return "정상 상태";
                                        }
                                        color: {
                                            var lvl = alertBox.alertLevel;
                                            if (lvl === 2)
                                                return Style.colorDanger;
                                            if (lvl === 1)
                                                return Style.colorSlate800;
                                            return "#166534";
                                        }
                                        font.bold: true
                                        font.pixelSize: 11
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        wrapMode: Text.WordWrap
                                        text: {
                                            if (!dashboardRoot.airStatsData)
                                                return "센서 데이터를 불러오는 중입니다...";
                                            var coWarn = dashboardRoot.airStatsData.co_status && dashboardRoot.airStatsData.co_status.indexOf("정상") === -1 && dashboardRoot.airStatsData.co_status.indexOf("양호") === -1;
                                            var co2Warn = dashboardRoot.airStatsData.gas_status && dashboardRoot.airStatsData.gas_status.indexOf("정상") === -1 && dashboardRoot.airStatsData.gas_status.indexOf("양호") === -1;

                                            if (coWarn && co2Warn)
                                                return "이산화탄소 및 일산화탄소 농도가 높습니다. 즉각적인 환기가 필요합니다.";
                                            if (coWarn)
                                                return "일산화탄소 농도가 높습니다. 즉각적인 환기가 필요합니다.";
                                            if (co2Warn)
                                                return "이산화탄소 농도가 높습니다. 환기 팬을 강화하십시오.";
                                            return "역사 내 공기질이 매우 좋습니다.";
                                        }
                                        color: {
                                            var lvl = alertBox.alertLevel;
                                            if (lvl === 2)
                                                return Style.colorDanger;
                                            if (lvl === 1)
                                                return Style.colorSlate800;
                                            return Style.colorSlate600;
                                        }
                                        font.pixelSize: 12
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Second Row
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 300
                spacing: 20

                // Device Control
                Rectangle {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.preferredWidth: 4
                    color: Style.colorSurface
                    radius: 12
                    border.color: Style.colorSlate200
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 15

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "👆 디바이스 통합 제어"
                                font: Style.fontBold
                                color: Style.colorSlate800
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                            ColumnLayout {
                                spacing: 2
                                Text {
                                    text: dashboardRoot.isManualMode ? "수동 모드" : "자동 모드"
                                    color: Style.isDarkMode ? "#FFFFFF" : "#0F172A"
                                    font.bold: true
                                    font.pixelSize: 10
                                }
                                Rectangle {
                                    width: 48
                                    height: 24
                                    radius: 12
                                    color: dashboardRoot.isManualMode ? "#EAB308" : Style.colorSlate300

                                    Rectangle {
                                        id: modeKnob
                                        x: dashboardRoot.isManualMode ? 26 : 2
                                        y: 2
                                        width: 20
                                        height: 20
                                        radius: 10
                                        color: "white"
                                        Behavior on x {
                                            NumberAnimation {
                                                duration: 200
                                            }
                                        }
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            dashboardRoot.isManualMode = !dashboardRoot.isManualMode;
                                            console.log("Mode changed to:", dashboardRoot.isManualMode ? "Manual" : "Auto");
                                            if (client && client.sendDeviceCommand) {
                                                client.sendDeviceCommand("mode_control", dashboardRoot.isManualMode ? "manual" : "auto");
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // ROI Box
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 80
                            color: "#F8FAFC"
                            radius: 8
                            border.color: Style.colorSlate200
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 15
                                ColumnLayout {
                                    Text {
                                        text: "ROI (REGION OF INTEREST) 설정"
                                        color: Style.colorPrimary
                                        font.bold: true
                                        font.pixelSize: 10
                                    }
                                    Text {
                                        text: "구역별 혼잡도 분석 설정"
                                        color: "#64748B" // Persistent Slate 500
                                    }
                                }
                                Item {
                                    Layout.fillWidth: true
                                }
                                Button {
                                    text: "설정하기"
                                    background: Rectangle {
                                        color: Style.colorPrimary
                                        radius: 4
                                    }
                                    contentItem: Text {
                                        text: parent.text
                                        color: "white"
                                        font.bold: true
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 15
                            rowSpacing: 15

                            Repeater {
                                model: [
                                    {
                                        name: "환기 팬",
                                        device: "motor",
                                        icon: "🍃",
                                        active: false
                                    },
                                    {
                                        name: "안내 방송",
                                        device: "speaker",
                                        icon: "🔊",
                                        active: false
                                    },
                                    {
                                        name: "디지털",
                                        device: "digital_display",
                                        icon: "🖥️",
                                        active: false
                                    },
                                    {
                                        name: "야간 조명",
                                        device: "lighting",
                                        icon: "💡",
                                        active: false
                                    }
                                ]

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 50
                                    border.color: Style.colorSlate200
                                    radius: 8
                                    color: "white"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 15
                                        Text {
                                            text: modelData.icon
                                        }
                                        Text {
                                            text: modelData.name
                                            font.bold: true
                                            color: "#0F172A" // Persistent Slate 900
                                        }
                                        Item {
                                            Layout.fillWidth: true
                                        }
                                        Rectangle {
                                            width: 36
                                            height: 20
                                            radius: 10
                                            color: modelData.active ? Style.colorPrimary : Style.colorSlate300

                                            Rectangle {
                                                id: knob
                                                x: modelData.active ? 18 : 2
                                                y: 2
                                                width: 16
                                                height: 16
                                                radius: 8
                                                color: "white"
                                                Behavior on x {
                                                    NumberAnimation {
                                                        duration: 200
                                                    }
                                                }
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    var next = !modelData.active;
                                                    modelData.active = next;
                                                    console.log("Device control:", modelData.name, "->", next ? "on" : "off");
                                                    if (client && client.sendDeviceCommand) {
                                                        client.sendDeviceCommand(modelData.device, next ? "on" : "off");
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Congestion
                Rectangle {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.preferredWidth: 6
                    color: Style.colorSurface
                    radius: 12
                    border.color: Style.colorSlate200
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 15

                        Text {
                            text: "👥 실시간 혼잡도 현황 (8구간)"
                            font: Style.fontBold
                            color: Style.colorSlate800
                        }

                        // 8-Section Timeline
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 80
                            spacing: 4

                            Repeater {
                                id: timelineRepeater
                                model: 8

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    radius: 6
                                    color: {
                                        if (dashboardRoot.sectionSums && dashboardRoot.sectionSums.length > index) {
                                            return dashboardRoot.getCongestionColor(dashboardRoot.sectionSums[index]);
                                        }
                                        return Style.colorSlate200;
                                    }

                                    ColumnLayout {
                                        anchors.centerIn: parent
                                        spacing: 2

                                        Text {
                                            text: (index + 1).toString()
                                            font.pixelSize: 18
                                            font.bold: true
                                            color: "white"
                                            Layout.alignment: Qt.AlignHCenter
                                        }

                                        Text {
                                            text: {
                                                if (dashboardRoot.sectionSums && dashboardRoot.sectionSums.length > index) {
                                                    return Math.round(dashboardRoot.sectionSums[index]) + "명";
                                                }
                                                return "-";
                                            }
                                            font.pixelSize: 10
                                            color: "white"
                                            Layout.alignment: Qt.AlignHCenter
                                        }
                                    }

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
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 20

                            RowLayout {
                                spacing: 6
                                Rectangle {
                                    width: 10
                                    height: 10
                                    radius: 5
                                    color: "#22C55E"
                                }
                                Text {
                                    text: "여유"
                                    color: Style.colorSlate600
                                    font.pixelSize: 11
                                }
                            }
                            RowLayout {
                                spacing: 6
                                Rectangle {
                                    width: 10
                                    height: 10
                                    radius: 5
                                    color: "#EAB308"
                                }
                                Text {
                                    text: "보통"
                                    color: Style.colorSlate600
                                    font.pixelSize: 11
                                }
                            }
                            RowLayout {
                                spacing: 6
                                Rectangle {
                                    width: 10
                                    height: 10
                                    radius: 5
                                    color: "#EF4444"
                                }
                                Text {
                                    text: "혼잡"
                                    color: Style.colorSlate600
                                    font.pixelSize: 11
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            property string currentDensity: {
                                var totalAvg = 0;
                                var activeSections = 0;
                                for (var i = 0; i < 8; i++) {
                                    if (sectionAverages && sectionAverages[i] > 0) {
                                        totalAvg += sectionAverages[i];
                                        activeSections++;
                                    }
                                }
                                var avg = activeSections > 0 ? (totalAvg / activeSections) : 0;
                                return Math.round(avg) + "%";
                            }

                            Repeater {
                                model: [
                                    {
                                        label: "현재 인원",
                                        val: grandTotalOccupants + "명",
                                        color: "#1E293B" // Persistent Slate 800
                                    },
                                    {
                                        label: "밀집도",
                                        val: parent.currentDensity,
                                        color: Style.colorPrimary
                                    },
                                    {
                                        label: "대기 시간",
                                        val: "3분",
                                        color: "#1E293B" // Persistent Slate 800
                                    }
                                ]
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 60
                                    color: "#F8FAFC" // Slate 50
                                    radius: 8
                                    ColumnLayout {
                                        anchors.centerIn: parent
                                        spacing: 4
                                        Text {
                                            text: modelData.label
                                            color: "#64748B" // Persistent Slate 500
                                            font.pixelSize: 10
                                            Layout.alignment: Qt.AlignHCenter
                                        }
                                        Text {
                                            text: modelData.val
                                            color: modelData.color
                                            font.bold: true
                                            font.pixelSize: 16
                                            Layout.alignment: Qt.AlignHCenter
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Timeline
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                color: Style.colorSurface
                radius: 12
                border.color: Style.colorSlate200
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 10

                    Text {
                        text: "📊 전체 디바이스 통합 상태 (REAL-TIME TIMELINE)"
                        font {
                            family: Style.fontBold.family
                            bold: true
                            pixelSize: 11
                        }
                        color: Style.colorSlate500
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Repeater {
                            model: 20
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                // Random-like pattern based on index
                                color: index < 5 ? "#22C55E" : (index < 10 ? "#EAB308" : (index < 15 ? "#22C55E" : "#EF4444"))
                            }
                        }
                    }

                    RowLayout {
                        spacing: 20
                        RowLayout {
                            Rectangle {
                                width: 10
                                height: 10
                                radius: 5
                                color: "#22C55E"
                            }
                            Text {
                                text: "정상 (Normal)"
                                color: Style.colorSlate500
                            }
                        }
                        RowLayout {
                            Rectangle {
                                width: 10
                                height: 10
                                radius: 5
                                color: "#EAB308"
                            }
                            Text {
                                text: "주의 (Caution)"
                                color: Style.colorSlate500
                            }
                        }
                        RowLayout {
                            Rectangle {
                                width: 10
                                height: 10
                                radius: 5
                                color: "#EF4444"
                            }
                            Text {
                                text: "위험 (Critical)"
                                color: Style.colorSlate500
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Text {
                            text: "전체 디바이스 가동률: <font color='#F97316'>94.2%</font>"
                            font.bold: true
                            color: Style.colorSlate800
                            textFormat: Text.RichText
                        }
                    }
                }
            }
        }
    }
}
