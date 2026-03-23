import QtQuick 2.15
import QtQuick.Shapes 1.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import ".."

ColumnLayout {
    id: dashboardRoot
    anchors.fill: parent
    spacing: 0

    property var airStatsData: ({})
    property var tempHumiData: ({})

    // 개별 반응형 프로퍼티 (데이터 수신 확인용)
    property double tempValue: 0.0
    property double humiValue: 0.0
    property double co2Value: 0.0
    property double coValue: 0.0
    property string lastEnvUpdate: "수신 대기 중..."

    property var sectionAverages: []
    property var sectionSums: []
    property var zoneCounts: []
    property int grandTotalOccupants: 0
    property bool isManualMode: false

    property string cam1Source: ""
    property string cam2Source: ""
    property string cam3Source: ""
    property string cam4Source: ""

    property bool showBoundingBox: false
    property var cam1Objects: []
    property var cam2Objects: []
    property var cam3Objects: []
    property var cam4Objects: []

    // ROI 데이터 (서버에서 수신한 전체 존 목록)
    property var roiZones: []

    // 카메라 인덱스(0-3) → camera_id 문자열 매핑
    // 수신한 zones의 camera_id 등장 순서 기준으로 자동 결정
    function getRoiCameraStringId(camIdx) {
        var seen = [];
        for (var i = 0; i < roiZones.length; i++) {
            var cid = roiZones[i].camera_id;
            if (seen.indexOf(cid) === -1)
                seen.push(cid);
        }
        return camIdx < seen.length ? seen[camIdx] : "";
    }

    function getZonesForCamera(camIdx) {
        var cid = getRoiCameraStringId(camIdx);
        if (cid === "")
            return [];
        return roiZones.filter(function (z) {
            return z.camera_id === cid;
        });
    }

    property int cam1Count: 0
    property int cam2Count: 0
    property int cam3Count: 0
    property int cam4Count: 0

    property int cam1Fps: 0
    property int cam2Fps: 0
    property int cam3Fps: 0
    property int cam4Fps: 0

    property int _cam1Count: 0
    property int _cam2Count: 0
    property int _cam3Count: 0
    property int _cam4Count: 0

    property int cam1RenderFps: 0
    property int cam2RenderFps: 0
    property int cam3RenderFps: 0
    property int cam4RenderFps: 0

    property int _cam1RenderCount: 0
    property int _cam2RenderCount: 0
    property int _cam3RenderCount: 0
    property int _cam4RenderCount: 0

    Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: {
            dashboardRoot.cam1Fps = dashboardRoot._cam1Count;
            dashboardRoot.cam2Fps = dashboardRoot._cam2Count;
            dashboardRoot.cam3Fps = dashboardRoot._cam3Count;
            dashboardRoot.cam4Fps = dashboardRoot._cam4Count;

            dashboardRoot._cam1Count = 0;
            dashboardRoot._cam2Count = 0;
            dashboardRoot._cam3Count = 0;
            dashboardRoot._cam4Count = 0;
            dashboardRoot._cam1RenderCount = 0;
            dashboardRoot._cam2RenderCount = 0;
            dashboardRoot._cam3RenderCount = 0;
            dashboardRoot._cam4RenderCount = 0;
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
        if (sumCount < networkClient.congestionEasyMax)
            return "#22C55E";
        else if (sumCount < networkClient.congestionNormalMax)
            return "#EAB308";
        else
            return "#EF4444";
    }

    function getCamCount(index) {
        if (index === 0)
            return dashboardRoot.cam1Count;
        if (index === 1)
            return dashboardRoot.cam2Count;
        if (index === 2)
            return dashboardRoot.cam3Count;
        if (index === 3)
            return dashboardRoot.cam4Count;
        return 0;
    }

    function getCamObjects(index) {
        if (index === 0) return dashboardRoot.cam1Objects;
        if (index === 1) return dashboardRoot.cam2Objects;
        if (index === 2) return dashboardRoot.cam3Objects;
        if (index === 3) return dashboardRoot.cam4Objects;
        return [];
    }

    Connections {
        target: networkClient
        function onIsConnectedChanged() {
            console.log("Dashboard - Connected: " + networkClient.isConnected);
        }
        function onStatusMessageChanged() {
            console.log("Dashboard - Status: " + networkClient.statusMessage);
        }
        function onAirStatsReceived(data) {
            console.log("Dashboard - Historical Air Stats Received: " + data.length);
        }
        function onRealtimeAirReceived(data) {
            console.log("Dashboard - Real-time Air Data Received: " + JSON.stringify(data));
            dashboardRoot.airStatsData = data;

            if (data.co2_ppm !== undefined)
                dashboardRoot.co2Value = data.co2_ppm;
            if (data.co_level !== undefined)
                dashboardRoot.coValue = data.co_level;
            if (data.temp !== undefined)
                dashboardRoot.tempValue = data.temp;
            if (data.humi !== undefined)
                dashboardRoot.humiValue = data.humi;

            let now = new Date();
            dashboardRoot.lastEnvUpdate = now.getHours().toString().padStart(2, '0') + ":" + now.getMinutes().toString().padStart(2, '0') + ":" + now.getSeconds().toString().padStart(2, '0');
        }
        function onZoneCongestionReceived(zones, totalCount, counts) {
            dashboardRoot.sectionSums = zones;
            dashboardRoot.grandTotalOccupants = totalCount;
            dashboardRoot.zoneCounts = counts;
        }
        function onCameraFrameReceived(cameraId, timestamp, metadata) {
            let url = "image://camera/" + cameraId + "?t=" + timestamp;
            let objectCount = metadata.count || 0;
            if (cameraId === 1) {
                dashboardRoot._cam1Count++;
                dashboardRoot.cam1Source = url;
                dashboardRoot.cam1Count = objectCount;
                dashboardRoot.cam1Objects = metadata.objs || [];
            } else if (cameraId === 2) {
                dashboardRoot._cam2Count++;
                dashboardRoot.cam2Source = url;
                dashboardRoot.cam2Count = objectCount;
                dashboardRoot.cam2Objects = metadata.objs || [];
            } else if (cameraId === 3) {
                dashboardRoot._cam3Count++;
                dashboardRoot.cam3Source = url;
                dashboardRoot.cam3Count = objectCount;
                dashboardRoot.cam3Objects = metadata.objs || [];
            } else if (cameraId === 4) {
                dashboardRoot._cam4Count++;
                dashboardRoot.cam4Source = url;
                dashboardRoot.cam4Count = objectCount;
                dashboardRoot.cam4Objects = metadata.objs || [];
            }
        }
        function onTempHumiReceived(data) {
            console.log("Dashboard - Temp/Humi Received: " + JSON.stringify(data));
            dashboardRoot.tempHumiData = data;

            if (data.temperature !== undefined)
                dashboardRoot.tempValue = data.temperature;
            if (data.humidity !== undefined)
                dashboardRoot.humiValue = data.humidity;

            let now = new Date();
            dashboardRoot.lastEnvUpdate = now.getHours().toString().padStart(2, '0') + ":" + now.getMinutes().toString().padStart(2, '0') + ":" + now.getSeconds().toString().padStart(2, '0');
        }
        function onRoiListReceived(zones) {
            dashboardRoot.roiZones = zones;
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 10

        // Top Row: Cameras + Environment
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

                // Camera Card
                Rectangle {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
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
                                text: dashboardRoot.showBoundingBox ? "객체 인식 ON" : "객체 인식 OFF"
                                color: dashboardRoot.showBoundingBox ? "#22C55E" : Style.colorPrimary
                                font.pixelSize: 12
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        dashboardRoot.showBoundingBox = !dashboardRoot.showBoundingBox;
                                    }
                                }
                            }
                        }

                        GridLayout {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: 12
                            columnSpacing: 12

                            Repeater {
                                model: 4
                                Rectangle {
                                    id: camContainer
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: Style.isDarkMode ? "#1e293b" : "#0f172a"
                                    radius: 12
                                    clip: true
                                    border.color: Style.isDarkMode ? "white" : "#334155"
                                    border.width: 1

                                    Image {
                                        id: cameraImageBack
                                        anchors.fill: parent
                                        fillMode: Image.PreserveAspectCrop
                                        smooth: false
                                        cache: false
                                        visible: {
                                            let fps = [dashboardRoot.cam1Fps, dashboardRoot.cam2Fps, dashboardRoot.cam3Fps, dashboardRoot.cam4Fps];
                                            return fps[index] > 0;
                                        }
                                    }

                                    Image {
                                        id: cameraImageFront
                                        anchors.fill: parent
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                        smooth: false
                                        cache: false
                                        visible: {
                                            let fps = [dashboardRoot.cam1Fps, dashboardRoot.cam2Fps, dashboardRoot.cam3Fps, dashboardRoot.cam4Fps];
                                            return fps[index] > 0 && status == Image.Ready;
                                        }
                                        source: {
                                            if (index === 0)
                                                return dashboardRoot.cam1Source;
                                            if (index === 1)
                                                return dashboardRoot.cam2Source;
                                            if (index === 2)
                                                return dashboardRoot.cam3Source;
                                            if (index === 3)
                                                return dashboardRoot.cam4Source;
                                            return "";
                                        }
                                        opacity: 1.0
                                        onStatusChanged: {
                                            if (status === Image.Ready) {
                                                cameraImageBack.source = source;
                                                if (index === 0)
                                                    dashboardRoot._cam1RenderCount++;
                                                else if (index === 1)
                                                    dashboardRoot._cam2RenderCount++;
                                                else if (index === 2)
                                                    dashboardRoot._cam3RenderCount++;
                                                else if (index === 3)
                                                    dashboardRoot._cam4RenderCount++;
                                            }
                                            if (status === Image.Error) {
                                                source = source;  // 재시도 방지
                                            }
                                        }
                                    }

                                    // Placeholder / No Signal state
                                    Rectangle {
                                        anchors.fill: parent
                                        color: "transparent"
                                        visible: {
                                            let fps = [dashboardRoot.cam1Fps, dashboardRoot.cam2Fps, dashboardRoot.cam3Fps, dashboardRoot.cam4Fps];
                                            return fps[index] === 0;
                                        }

                                        ColumnLayout {
                                            anchors.centerIn: parent
                                            spacing: 8
                                            Rectangle {
                                                width: 48
                                                height: 48
                                                radius: 24
                                                color: "#1e293b"
                                                Layout.alignment: Qt.AlignCenter
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "!"
                                                    font.pixelSize: 24
                                                    color: Style.colorDanger
                                                    font.bold: true
                                                }
                                            }
                                            Text {
                                                text: "NO SIGNAL"
                                                color: "#94A3B8"
                                                font.pixelSize: 12
                                                font.bold: true
                                                Layout.alignment: Qt.AlignCenter
                                            }
                                            Text {
                                                text: "CAM-0" + (index + 1)
                                                color: Style.colorSlate500
                                                font.pixelSize: 10
                                                Layout.alignment: Qt.AlignCenter
                                            }
                                        }
                                    }

                                    Image {
                                        id: cameraImage
                                        anchors.fill: parent
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: false
                                        smooth: false
                                        cache: false
                                        source: {
                                            if (index === 0)
                                                return dashboardRoot.cam1Source;
                                            if (index === 1)
                                                return dashboardRoot.cam2Source;
                                            if (index === 2)
                                                return dashboardRoot.cam3Source;
                                            if (index === 3)
                                                return dashboardRoot.cam4Source;
                                            return "";
                                        }
                                        visible: {
                                            let fps = [dashboardRoot.cam1Fps, dashboardRoot.cam2Fps, dashboardRoot.cam3Fps, dashboardRoot.cam4Fps];
                                            return fps[index] > 0;
                                        }
                                        opacity: 1.0
                                        onStatusChanged: {
                                            // 로딩 실패해도 이전 이미지 유지 (검은 화면 방지)
                                            if (status === Image.Error) {
                                                source = source;  // 재시도 방지
                                            }
                                        }
                                    }

                                    // Overlay Shadow
                                    Rectangle {
                                        anchors.fill: parent
                                        gradient: Gradient {
                                            GradientStop {
                                                position: 0.0
                                                color: "#44000000"
                                            }
                                            GradientStop {
                                                position: 0.2
                                                color: "transparent"
                                            }
                                            GradientStop {
                                                position: 0.8
                                                color: "transparent"
                                            }
                                            GradientStop {
                                                position: 1.0
                                                color: "#88000000"
                                            }
                                        }
                                    }

                                    // Status Badge (LIVE / OFF)
                                    Rectangle {
                                        property bool isLive: {
                                            let fps = [dashboardRoot.cam1Fps, dashboardRoot.cam2Fps, dashboardRoot.cam3Fps, dashboardRoot.cam4Fps];
                                            return fps[index] > 0;
                                        }
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.margins: 12
                                        width: isLive ? 54 : 44
                                        height: 22
                                        radius: 6
                                        color: isLive ? "#cc22c55e" : "#cc475569"
                                        border.color: "#33ffffff"
                                        border.width: 1

                                        RowLayout {
                                            anchors.centerIn: parent
                                            spacing: 5
                                            Rectangle {
                                                width: 8
                                                height: 8
                                                radius: 4
                                                color: "white"
                                                visible: parent.parent.isLive
                                                SequentialAnimation on opacity {
                                                    loops: Animation.Infinite
                                                    NumberAnimation {
                                                        from: 1
                                                        to: 0.3
                                                        duration: 800
                                                    }
                                                    NumberAnimation {
                                                        from: 0.3
                                                        to: 1
                                                        duration: 800
                                                    }
                                                }
                                            }
                                            Text {
                                                text: parent.parent.isLive ? "LIVE" : "OFF"
                                                color: "white"
                                                font.pixelSize: 11
                                                font.bold: true
                                                font.letterSpacing: 0.5
                                            }
                                        }
                                    }

                                    // Object Count Badge
                                    Rectangle {
                                        visible: {
                                            let fps = [dashboardRoot.cam1Fps, dashboardRoot.cam2Fps, dashboardRoot.cam3Fps, dashboardRoot.cam4Fps];
                                            return fps[index] > 0 && getCamCount(index) > 0;
                                        }
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.topMargin: 40
                                        anchors.leftMargin: 12
                                        width: 80
                                        height: 22
                                        radius: 6
                                        color: "#cc0f172a"
                                        border.color: "#44ffffff"
                                        border.width: 1

                                        Text {
                                            anchors.centerIn: parent
                                            text: "DETECTED: " + getCamCount(index)
                                            color: "#E2E8F0"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }

                                    // Bounding Box Overlay
                                    Repeater {
                                        model: {
                                            let fps = [dashboardRoot.cam1Fps, dashboardRoot.cam2Fps, dashboardRoot.cam3Fps, dashboardRoot.cam4Fps];
                                            if (dashboardRoot.showBoundingBox && fps[index] > 0) {
                                                return getCamObjects(index);
                                            }
                                            return [];
                                        }
                                        delegate: Rectangle {
                                            x: modelData.x * camContainer.width
                                            y: modelData.y * camContainer.height
                                            width: modelData.w * camContainer.width
                                            height: modelData.h * camContainer.height
                                            color: "transparent"
                                            border.color: "#00FF00"
                                            border.width: 2
                                            radius: 2
                                        }
                                    }

                                    // Camera Label
                                    Text {
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        anchors.margins: 12
                                        // text: "CAM-0" + (index + 1)
                                        text: {
                                            let fps = [dashboardRoot.cam1Fps, dashboardRoot.cam2Fps, dashboardRoot.cam3Fps, dashboardRoot.cam4Fps];
                                            return "CAM-0" + (index + 1) + "  " + fps[index] + "fps";
                                        }
                                        color: "white"
                                        font.pixelSize: 12
                                        font.bold: true
                                        style: Text.Outline
                                        styleColor: "black"
                                    }

                                    // Hover overlay
                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true

                                        onClicked: {
                                            let fps = [dashboardRoot.cam1Fps, dashboardRoot.cam2Fps, dashboardRoot.cam3Fps, dashboardRoot.cam4Fps];
                                            if (fps[index] > 0) {
                                                // Repeater의 현재 index를 팝업에 전달하여 실시간 바인딩 활성화
                                                expandedCameraPopup.currentCameraIndex = index;
                                                expandedCameraPopup.currentTitle = "CAM-0" + (index + 1) + " 확대화면";
                                                expandedCameraPopup.open();
                                            } else {
                                                console.log("카메라 신호가 없어 확대할 수 없습니다.");
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
                    Layout.preferredWidth: 350
                    color: Style.colorSurface
                    radius: 12
                    border.color: Style.colorSlate200
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 20

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "🍂 역사 내 환경 모니터링"
                                font: Style.fontBold
                                color: Style.colorSlate800
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                            Text {
                                text: "수신: " + dashboardRoot.lastEnvUpdate
                                font.pixelSize: 10
                                color: Style.colorSlate500
                            }
                        }

                        // 온습도
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 20

                            RowLayout {
                                spacing: 6
                                Text {
                                    text: "🌡️ 온도"
                                    color: Style.colorSlate500
                                    font: Style.fontSmall
                                }
                                Text {
                                    text: dashboardRoot.tempValue > 0 ? dashboardRoot.tempValue.toFixed(1) + " °C" : "-- °C"
                                    font: Style.fontLarge
                                    color: Style.colorSlate800
                                }
                            }

                            Rectangle {
                                width: 1
                                height: 20
                                color: Style.colorSlate200
                            }

                            RowLayout {
                                spacing: 6
                                Text {
                                    text: "💧 습도"
                                    color: Style.colorSlate500
                                    font: Style.fontSmall
                                }
                                Text {
                                    text: dashboardRoot.humiValue > 0 ? dashboardRoot.humiValue.toFixed(1) + " %" : "-- %"
                                    font: Style.fontLarge
                                    color: Style.colorSlate800
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Style.colorSlate200
                        }

                        // CO2
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            RowLayout {
                                Text {
                                    text: "대기질"
                                    color: Style.colorSlate500
                                    font: Style.fontSmall
                                }
                                Item {
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: dashboardRoot.co2Value > 0 ? dashboardRoot.co2Value.toFixed(1) : "0"
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
                                    width: Math.min(parent.width, (dashboardRoot.co2Value > 0 ? (dashboardRoot.co2Value / 1200) * parent.width : 0))
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

                            Text {
                                text: "💡 대기질: 400ppm 이상 '주의', 600ppm 이상 '위험' 단계 진입"
                                font.pixelSize: 10
                                color: Style.colorSlate500
                            }
                        }

                        // AQI
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            RowLayout {
                                Text {
                                    text: "일산화탄소 농도"
                                    color: Style.colorSlate500
                                    font: Style.fontSmall
                                }
                                Item {
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: dashboardRoot.coValue > 0 ? dashboardRoot.coValue.toFixed(2) : "0"
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
                                    width: Math.min(parent.width, (dashboardRoot.coValue > 0 ? (dashboardRoot.coValue / 50) * parent.width : 0))
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

                            Text {
                                text: "💡 CO: 9ppm 이상 '주의', 25ppm 이상 '위험' 단계 진입"
                                font.pixelSize: 10
                                color: Style.colorSlate500
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
                spacing: 10

                // Congestion
                Rectangle {
                    Layout.preferredHeight: deviceControlInner.implicitHeight + 35
                    Layout.alignment: Qt.AlignTop
                    Layout.fillWidth: true
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
                                    color: (dashboardRoot.zoneCounts && dashboardRoot.zoneCounts.length > index) ? dashboardRoot.getCongestionColor(dashboardRoot.zoneCounts[index]) : ((dashboardRoot.sectionSums && dashboardRoot.sectionSums.length > index) ? dashboardRoot.getCongestionColor(dashboardRoot.sectionSums[index]) : Style.colorSlate200)

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
                                                if (dashboardRoot.zoneCounts && dashboardRoot.zoneCounts.length > index)
                                                    return dashboardRoot.zoneCounts[index] + "명";
                                                return "0명";
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
                                // 열차 정원 (Protocol::TOTAL_CAPACITY) 대비 전체 인원 밀집도 계산
                                var capacity = networkClient.totalCapacity;
                                if (capacity <= 0)
                                    return "0%";
                                var avg = (grandTotalOccupants / capacity) * 100;
                                return Math.min(100, Math.round(avg)) + "%";
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

                // Device Control
                Rectangle {
                    Layout.preferredWidth: 350
                    Layout.preferredHeight: deviceControlInner.implicitHeight + 35
                    Layout.alignment: Qt.AlignTop
                    color: Style.colorSurface
                    radius: 12
                    border.color: Style.colorSlate200
                    border.width: 1

                    ColumnLayout {
                        id: deviceControlInner
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.topMargin: 15
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.rightMargin: 15
                            Text {
                                text: "👆 디바이스 통합 제어"
                                font: Style.fontBold
                                color: Style.colorSlate800
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                            Text {
                                text: dashboardRoot.isManualMode ? "수동 모드" : "자동 모드"
                                color: Style.isDarkMode ? "#FFFFFF" : "#0F172A"
                                font.bold: true
                                font.pixelSize: 10
                            }
                            Rectangle {
                                width: 36
                                height: 20
                                radius: 10
                                color: dashboardRoot.isManualMode ? "#EAB308" : Style.colorSlate300

                                Rectangle {
                                    id: modeKnob
                                    x: dashboardRoot.isManualMode ? 18 : 2
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
                                        dashboardRoot.isManualMode = !dashboardRoot.isManualMode;
                                        console.log("Mode changed to:", dashboardRoot.isManualMode ? "Manual" : "Auto");
                                        if (networkClient && networkClient.sendDeviceCommand) {
                                            networkClient.sendDeviceCommand(networkClient.DEVICE_MODE_CONTROL, dashboardRoot.isManualMode ? networkClient.ACTION_MANUAL : networkClient.ACTION_AUTO);
                                        }
                                    }
                                }
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 1
                            columnSpacing: 15
                            rowSpacing: 10

                            Repeater {
                                model: [
                                    {
                                        name: "환기 팬",
                                        device: networkClient.DEVICE_MOTOR,
                                        icon: "🍃",
                                        type: "toggle",
                                        options: []
                                    },
                                    {
                                        name: "안내 방송",
                                        device: networkClient.DEVICE_SPEAKER,
                                        icon: "🔊",
                                        type: "buttons",
                                        options: ["1", "2", "3", "4"]
                                    },
                                    {
                                        name: "디스플레이",
                                        device: networkClient.DEVICE_DIGITAL_DISPLAY,
                                        icon: "🖥️",
                                        type: "buttons",
                                        options: ["1", "2", "3"]
                                    }
                                ]

                                Rectangle {
                                    id: deviceItem
                                    property var deviceData: modelData
                                    property bool isActive: false
                                    property string activeOption: ""

                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 50
                                    border.color: Style.colorSlate200
                                    radius: 8
                                    color: "white"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 15
                                        Text {
                                            text: deviceItem.deviceData.icon
                                        }
                                        Text {
                                            text: deviceItem.deviceData.name
                                            font: Style.fontBold
                                            color: "#0F172A"
                                        }
                                        Item {
                                            Layout.fillWidth: true
                                        }

                                        // 1) Toggle switch
                                        Rectangle {
                                            visible: deviceItem.deviceData.type === "toggle"
                                            width: 36
                                            height: 20
                                            radius: 10
                                            color: deviceItem.isActive ? Style.colorPrimary : Style.colorSlate300

                                            Rectangle {
                                                id: knob
                                                x: deviceItem.isActive ? 18 : 2
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
                                                    deviceItem.isActive = !deviceItem.isActive;
                                                    if (networkClient && networkClient.sendDeviceCommand) {
                                                        networkClient.sendDeviceCommand(deviceItem.deviceData.device, deviceItem.isActive ? networkClient.ACTION_ON : networkClient.ACTION_OFF);
                                                    }
                                                }
                                            }
                                        }

                                        // 2) Numbered buttons
                                        RowLayout {
                                            visible: deviceItem.deviceData.type === "buttons"
                                            spacing: 6

                                            Timer {
                                                id: resetTimer
                                                interval: 3000
                                                repeat: false
                                                onTriggered: {
                                                    deviceItem.activeOption = "";
                                                }
                                            }

                                            Repeater {
                                                model: deviceItem.deviceData.type === "buttons" ? deviceItem.deviceData.options : []
                                                Rectangle {
                                                    width: 28
                                                    height: 28
                                                    radius: 6
                                                    color: deviceItem.activeOption === modelData ? Style.colorPrimary : "white"
                                                    border.color: deviceItem.activeOption === modelData ? Style.colorPrimary : Style.colorSlate300
                                                    border.width: 1

                                                    Text {
                                                        anchors.centerIn: parent
                                                        text: modelData
                                                        color: deviceItem.activeOption === modelData ? "white" : "#475569"
                                                        font.bold: true
                                                        font.pixelSize: 13
                                                    }

                                                    MouseArea {
                                                        anchors.fill: parent
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: {
                                                            var val = modelData;
                                                            deviceItem.activeOption = val;
                                                            resetTimer.restart();
                                                            if (networkClient && networkClient.sendDeviceCommand) {
                                                                var targetAction = networkClient.ACTION_1;
                                                                if (val === "2")
                                                                    targetAction = networkClient.ACTION_2;
                                                                else if (val === "3")
                                                                    targetAction = networkClient.ACTION_3;
                                                                else if (val === "4")
                                                                    targetAction = networkClient.ACTION_4;
                                                                networkClient.sendDeviceCommand(deviceItem.deviceData.device, targetAction);
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
                    }
                }
            }
        }
    RoiEditorDialog {
        id: roiEditorDialog
    }

    // Dashboard.qml 하단에 추가
    Popup {
        id: expandedCameraPopup
        parent: Overlay.overlay
        width: parent.width * 0.9
        height: parent.height * 0.9
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        // 정적 소스 대신 카메라 인덱스를 받아 동적 바인딩에 사용
        property int currentCameraIndex: -1
        property string currentTitle: ""

        background: Rectangle {
            color: "black"
            radius: 10
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            Text {
                text: expandedCameraPopup.currentTitle
                color: "white"
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }

            // 팝업 내부 더블 버퍼링 렌더링 영역
            Item {
                id: expandedImageContainer
                Layout.fillWidth: true
                Layout.fillHeight: true

                Image {
                    id: expandedImageBack
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    cache: false
                    visible: true
                }

                Image {
                    id: expandedImageFront
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    smooth: true
                    cache: false
                    visible: status === Image.Ready

                    // 메인 화면과 동일하게 dashboardRoot의 camXSource 변경을 실시간 감지
                    source: {
                        if (expandedCameraPopup.currentCameraIndex === 0)
                            return dashboardRoot.cam1Source;
                        if (expandedCameraPopup.currentCameraIndex === 1)
                            return dashboardRoot.cam2Source;
                        if (expandedCameraPopup.currentCameraIndex === 2)
                            return dashboardRoot.cam3Source;
                        if (expandedCameraPopup.currentCameraIndex === 3)
                            return dashboardRoot.cam4Source;
                        return "";
                    }

                    // 로딩 완료 시 백버퍼 업데이트로 깜빡임 제거
                    onStatusChanged: {
                        if (status === Image.Ready) {
                            expandedImageBack.source = source;
                        }
                    }
                }

                // Bounding Box Overlay (PreserveAspectFit 보정)
                Repeater {
                    model: {
                        let fps = [dashboardRoot.cam1Fps, dashboardRoot.cam2Fps, dashboardRoot.cam3Fps, dashboardRoot.cam4Fps];
                        let camIdx = expandedCameraPopup.currentCameraIndex;
                        if (dashboardRoot.showBoundingBox && camIdx >= 0 && fps[camIdx] > 0) {
                            return getCamObjects(camIdx);
                        }
                        return [];
                    }
                    delegate: Rectangle {
                        property real imgX: (expandedImageContainer.width - expandedImageFront.paintedWidth) / 2
                        property real imgY: (expandedImageContainer.height - expandedImageFront.paintedHeight) / 2
                        x: imgX + modelData.x * expandedImageFront.paintedWidth
                        y: imgY + modelData.y * expandedImageFront.paintedHeight
                        width: modelData.w * expandedImageFront.paintedWidth
                        height: modelData.h * expandedImageFront.paintedHeight
                        color: "transparent"
                        border.color: "#00FF00"
                        border.width: 2
                        radius: 2
                    }
                }
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 20
                spacing: 12

                // ROI 설정 버튼
                Rectangle {
                    width: 140
                    height: 44
                    radius: height / 2
                    color: roiMouseArea.pressed ? "#1d4ed8" : (roiMouseArea.containsMouse ? "#3b82f6" : "#2563eb")
                    Behavior on color {
                        ColorAnimation {
                            duration: 150
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "승강장 구역 설정"
                        color: "white"
                        font.pixelSize: 15
                        font.bold: true
                        font.letterSpacing: 1.0
                    }

                    MouseArea {
                        id: roiMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            var camIdx = expandedCameraPopup.currentCameraIndex;
                            var camStringId = dashboardRoot.getRoiCameraStringId(camIdx);
                            var zones = dashboardRoot.getZonesForCamera(camIdx);
                            roiEditorDialog.openForCamera(camIdx, camStringId, zones);
                        }
                    }
                }

                // 닫기 버튼
                Rectangle {
                    width: 140
                    height: 44
                    radius: height / 2
                    color: closeMouseArea.pressed ? "#0F172A" : (closeMouseArea.containsMouse ? "#334155" : "#1E293B")
                    border.color: closeMouseArea.containsMouse ? "#94A3B8" : "#475569"
                    border.width: 1
                    Behavior on color {
                        ColorAnimation {
                            duration: 150
                        }
                    }
                    Behavior on border.color {
                        ColorAnimation {
                            duration: 150
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "닫기"
                        color: closeMouseArea.pressed ? "#94A3B8" : "white"
                        font.pixelSize: 15
                        font.bold: true
                        font.letterSpacing: 1.0
                    }

                    MouseArea {
                        id: closeMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            expandedCameraPopup.currentCameraIndex = -1;
                            expandedCameraPopup.close();
                        }
                    }
                }
            }
        }
    }
}
