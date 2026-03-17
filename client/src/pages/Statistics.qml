import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import ".."

ColumnLayout {
    width: parent.width
    height: parent.height
    spacing: 20

    function getDayName(day) {
        var days = ["", "일요일", "월요일", "화요일", "수요일", "목요일", "금요일", "토요일"];
        return days[day] ? days[day] : day;
    }

    function getCongestionColor(count) {
        if (count === undefined || count === null) return "#f1f5f9";
        if (count < networkClient.congestionEasyMax) return "#22C55E";
        if (count < networkClient.congestionNormalMax) return "#EAB308";
        return "#EF4444";
    }

    // Data grouped by day
    property var airStatsByDay: []
    property var flowStatsByDay: []
    property var tempHumiStatsByDay: []

    // Expanded state per day (keyed by String(day))
    property var expandedAirDays: ({})
    property var expandedFlowDays: ({})
    property var expandedTempHumiDays: ({})

    Connections {
        target: networkClient
        function onIsConnectedChanged() {
            console.log("Statistics - Connected: " + networkClient.isConnected);
        }
        function onStatusMessageChanged() {
            console.log("Statistics - Status: " + networkClient.statusMessage);
        }
        function onAirStatsReceived(data) {
            console.log("Statistics - Air Stats Received! Count: " + data.length);
            var dayGroups = {};
            for (var i = 0; i < data.length; i++) {
                var item = data[i];
                if (!item) continue;
                var day = item.day;
                if (!dayGroups.hasOwnProperty(day))
                    dayGroups[day] = { day: day, hoursMap: {} };
                dayGroups[day].hoursMap[item.hour] = item;
            }
            var arr = [];
            var dayKeys = Object.keys(dayGroups).sort(function(a, b) { return Number(a) - Number(b); });
            for (var d = 0; d < dayKeys.length; d++) {
                var dg = dayGroups[dayKeys[d]];
                var hourKeys = Object.keys(dg.hoursMap).sort(function(a, b) { return Number(a) - Number(b); });
                var hours = hourKeys.map(function(k) { return dg.hoursMap[k]; });
                var sumCo = 0, sumCo2 = 0;
                for (var h = 0; h < hours.length; h++) {
                    sumCo += hours[h].co || 0;
                    sumCo2 += hours[h].co2 || 0;
                }
                var n = hours.length || 1;
                arr.push({ day: dg.day, summary: { co: sumCo / n, co2: sumCo2 / n }, hours: hours });
            }
            airStatsByDay = arr;
        }
        function onFlowStatsReceived(data) {
            console.log("Statistics - Flow Stats Received! Count: " + data.length);
            if (data.length > 0) console.log("Flow Data[0]: " + JSON.stringify(data[0]));
            var dayGroups = {};
            for (var i = 0; i < data.length; i++) {
                var item = data[i];
                if (!item) continue;
                var day = item.day;
                if (!dayGroups.hasOwnProperty(day))
                    dayGroups[day] = { day: day, hourMap: {} };
                var hKey = String(item.hour);
                if (!dayGroups[day].hourMap.hasOwnProperty(hKey))
                    dayGroups[day].hourMap[hKey] = { hour: item.hour, platforms: [0,0,0,0,0,0,0,0], total: 0 };
                var pIdx = parseInt(item.platform) - 1;
                if (pIdx >= 0 && pIdx < 8) {
                    var count = item.avg_count !== undefined ? item.avg_count : 0;
                    dayGroups[day].hourMap[hKey].platforms[pIdx] = count;
                    dayGroups[day].hourMap[hKey].total += count;
                }
            }
            var arr = [];
            var dayKeys = Object.keys(dayGroups).sort(function(a, b) { return Number(a) - Number(b); });
            for (var d = 0; d < dayKeys.length; d++) {
                var dg = dayGroups[dayKeys[d]];
                var hourKeys = Object.keys(dg.hourMap).sort(function(a, b) { return Number(a) - Number(b); });
                var hours = hourKeys.map(function(k) { return dg.hourMap[k]; });
                var sumPlatforms = [0,0,0,0,0,0,0,0];
                var sumTotal = 0;
                for (var h = 0; h < hours.length; h++) {
                    for (var p = 0; p < 8; p++) sumPlatforms[p] += hours[h].platforms[p];
                    sumTotal += hours[h].total;
                }
                var n = hours.length || 1;
                arr.push({
                    day: dg.day,
                    summary: { platforms: sumPlatforms.map(function(v) { return v / n; }), total: sumTotal / n },
                    hours: hours
                });
            }
            flowStatsByDay = arr;
        }
        function onTempHumiStatsReceived(data) {
            console.log("Statistics - Temp/Humi Stats Received! Count: " + data.length);
            var dayGroups = {};
            for (var i = 0; i < data.length; i++) {
                var item = data[i];
                if (!item) continue;
                var day = item.day;
                if (!dayGroups.hasOwnProperty(day))
                    dayGroups[day] = { day: day, hoursMap: {} };
                dayGroups[day].hoursMap[item.hour] = item;
            }
            var arr = [];
            var dayKeys = Object.keys(dayGroups).sort(function(a, b) { return Number(a) - Number(b); });
            for (var d = 0; d < dayKeys.length; d++) {
                var dg = dayGroups[dayKeys[d]];
                var hourKeys = Object.keys(dg.hoursMap).sort(function(a, b) { return Number(a) - Number(b); });
                var hours = hourKeys.map(function(k) { return dg.hoursMap[k]; });
                var sumTemp = 0, sumHumi = 0;
                for (var h = 0; h < hours.length; h++) {
                    sumTemp += hours[h].temp || 0;
                    sumHumi += hours[h].humi || 0;
                }
                var n = hours.length || 1;
                arr.push({ day: dg.day, summary: { temp: sumTemp / n, humi: sumHumi / n }, hours: hours });
            }
            tempHumiStatsByDay = arr;
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
        Item { Layout.fillWidth: true }
    }

    // Tabs
    TabBar {
        id: bar
        Layout.fillWidth: true
        Layout.leftMargin: 20; Layout.rightMargin: 20
        background: Rectangle { color: Style.colorSurface; border.color: Style.colorTableBorder; border.width: 1 }

        TabButton {
            id: tab2; text: "🌫️ 공기질 통계"; Layout.fillWidth: true
            background: Rectangle { color: tab2.checked ? Style.colorSurface : Style.colorSlate200; border.color: Style.colorTableBorder; border.width: 1 }
            contentItem: Text { text: tab2.text; font: Style.fontRegular; color: tab2.checked ? Style.colorSlate800 : Style.colorSlate600; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideNone }
        }
        TabButton {
            id: tab4; text: "🌡️ 온습도 통계"; Layout.fillWidth: true
            background: Rectangle { color: tab4.checked ? Style.colorSurface : Style.colorSlate200; border.color: Style.colorTableBorder; border.width: 1 }
            contentItem: Text { text: tab4.text; font: Style.fontRegular; color: tab4.checked ? Style.colorSlate800 : Style.colorSlate600; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideNone }
        }
        TabButton {
            id: tab3; text: "👥 승객 흐름 통계"; Layout.fillWidth: true
            background: Rectangle { color: tab3.checked ? Style.colorSurface : Style.colorSlate200; border.color: Style.colorTableBorder; border.width: 1 }
            contentItem: Text { text: tab3.text; font: Style.fontRegular; color: tab3.checked ? Style.colorSlate800 : Style.colorSlate600; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideNone }
        }
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: 20; Layout.rightMargin: 20; Layout.bottomMargin: 20

        StackLayout {
            id: stackLayout
            anchors.fill: parent
            currentIndex: bar.currentIndex

            // ── Tab 0: Air Quality ──────────────────────────────────────────
            ScrollView {
                contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width
                    spacing: 0

                    // Column header
                    RowLayout {
                        Layout.fillWidth: true; spacing: 0
                        Rectangle { Layout.preferredWidth: 90; height: 40; color: Style.colorTableHeadAir; Text { anchors.centerIn: parent; text: "요일"; color: "white"; font.bold: true } }
                        Rectangle { Layout.fillWidth: true; height: 40; color: Style.colorTableHeadAir; Text { anchors.centerIn: parent; text: "CO 평균 (ppm)"; color: "white"; font.bold: true } }
                        Rectangle { Layout.fillWidth: true; height: 40; color: Style.colorTableHeadAir; Text { anchors.centerIn: parent; text: "CO2 평균 (ppm)"; color: "white"; font.bold: true } }
                        Rectangle { Layout.preferredWidth: 90; height: 40; color: Style.colorTableHeadAir; Text { anchors.centerIn: parent; text: "상태"; color: "white"; font.bold: true } }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: contentHeight
                        interactive: false
                        model: airStatsByDay

                        delegate: Column {
                            id: airDayDelegate
                            property var dayData: modelData
                            property string dayKey: String(dayData.day)
                            property bool expanded: expandedAirDays.hasOwnProperty(dayKey) && expandedAirDays[dayKey]
                            width: parent ? parent.width : 0

                            // Day summary row (clickable)
                            Rectangle {
                                width: parent.width; height: 48
                                color: airDayDelegate.expanded ? "#BFDBFE" : "#EFF6FF"
                                border.color: Style.colorTableBorder; border.width: 1

                                RowLayout {
                                    anchors.fill: parent; spacing: 0
                                    Rectangle {
                                        Layout.preferredWidth: 90; height: 48; color: "transparent"
                                        Text {
                                            anchors.centerIn: parent
                                            text: (airDayDelegate.expanded ? "▼  " : "▶  ") + getDayName(airDayDelegate.dayData.day)
                                            font.bold: true; color: Style.colorSlate800
                                        }
                                    }
                                    Rectangle {
                                        Layout.fillWidth: true; height: 48; color: "transparent"
                                        border.color: Style.colorTableBorder; border.width: 1
                                        Text { anchors.centerIn: parent; text: airDayDelegate.dayData.summary.co.toFixed(2); color: Style.colorSlate800 }
                                    }
                                    Rectangle {
                                        Layout.fillWidth: true; height: 48; color: "transparent"
                                        border.color: Style.colorTableBorder; border.width: 1
                                        Text { anchors.centerIn: parent; text: airDayDelegate.dayData.summary.co2.toFixed(2); color: Style.colorSlate800 }
                                    }
                                    Rectangle {
                                        Layout.preferredWidth: 90; height: 48; color: "transparent"
                                        border.color: Style.colorTableBorder; border.width: 1
                                        Text { anchors.centerIn: parent; text: airDayDelegate.dayData.hours.length + "개 시간대"; color: Style.colorSlate500; font.pixelSize: 11 }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        var copy = Object.assign({}, expandedAirDays);
                                        copy[airDayDelegate.dayKey] = !airDayDelegate.expanded;
                                        expandedAirDays = copy;
                                    }
                                }
                            }

                            // Hour rows (expanded)
                            Repeater {
                                model: airDayDelegate.expanded ? airDayDelegate.dayData.hours : []
                                delegate: RowLayout {
                                    id: airHourRow
                                    property var hourData: modelData
                                    width: airDayDelegate.width; height: 40; spacing: 0

                                    Rectangle { Layout.preferredWidth: 90; height: 40; color: Style.colorSurface; border.color: Style.colorTableBorder
                                        Text { anchors.centerIn: parent; text: airHourRow.hourData.hour + "시"; color: Style.colorSlate800 } }
                                    Rectangle { Layout.fillWidth: true; height: 40; color: Style.colorSurface; border.color: Style.colorTableBorder
                                        Text { anchors.centerIn: parent; text: (airHourRow.hourData.co !== undefined) ? airHourRow.hourData.co.toFixed(2) : "0.00"; color: Style.colorSlate800 } }
                                    Rectangle { Layout.fillWidth: true; height: 40; color: Style.colorSurface; border.color: Style.colorTableBorder
                                        Text { anchors.centerIn: parent; text: (airHourRow.hourData.co2 !== undefined) ? airHourRow.hourData.co2.toFixed(2) : "0.00"; color: Style.colorSlate800 } }
                                    Rectangle { Layout.preferredWidth: 90; height: 40; color: Style.colorSurface; border.color: Style.colorTableBorder
                                        Text { anchors.centerIn: parent; text: (airHourRow.hourData.co_status || "") + "/" + (airHourRow.hourData.gas_status || ""); color: Style.colorSlate800; font.pixelSize: 10 } }
                                }
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true; Layout.topMargin: 50
                        text: networkClient.isConnected ? "공기질 데이터가 없습니다." : "서버에 연결되지 않았습니다."
                        color: Style.colorSlate500; font: Style.fontNormal
                        horizontalAlignment: Text.AlignHCenter
                        visible: airStatsByDay.length === 0
                    }
                }
            }

            // ── Tab 1: Temp/Humi ────────────────────────────────────────────
            ScrollView {
                contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true; spacing: 0
                        Rectangle { Layout.preferredWidth: 90; height: 40; color: "#3B82F6"; Text { anchors.centerIn: parent; text: "요일"; color: "white"; font.bold: true } }
                        Rectangle { Layout.fillWidth: true; height: 40; color: "#3B82F6"; Text { anchors.centerIn: parent; text: "온도 평균 (°C)"; color: "white"; font.bold: true } }
                        Rectangle { Layout.fillWidth: true; height: 40; color: "#3B82F6"; Text { anchors.centerIn: parent; text: "습도 평균 (%)"; color: "white"; font.bold: true } }
                        Rectangle { Layout.preferredWidth: 90; height: 40; color: "#3B82F6"; Text { anchors.centerIn: parent; text: ""; color: "white"; font.bold: true } }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: contentHeight
                        interactive: false
                        model: tempHumiStatsByDay

                        delegate: Column {
                            id: thDayDelegate
                            property var dayData: modelData
                            property string dayKey: String(dayData.day)
                            property bool expanded: expandedTempHumiDays.hasOwnProperty(dayKey) && expandedTempHumiDays[dayKey]
                            width: parent ? parent.width : 0

                            Rectangle {
                                width: parent.width; height: 48
                                color: thDayDelegate.expanded ? "#BFDBFE" : "#EFF6FF"
                                border.color: Style.colorTableBorder; border.width: 1

                                RowLayout {
                                    anchors.fill: parent; spacing: 0
                                    Rectangle {
                                        Layout.preferredWidth: 90; height: 48; color: "transparent"
                                        Text {
                                            anchors.centerIn: parent
                                            text: (thDayDelegate.expanded ? "▼  " : "▶  ") + getDayName(thDayDelegate.dayData.day)
                                            font.bold: true; color: Style.colorSlate800
                                        }
                                    }
                                    Rectangle {
                                        Layout.fillWidth: true; height: 48; color: "transparent"
                                        border.color: Style.colorTableBorder; border.width: 1
                                        Text { anchors.centerIn: parent; text: thDayDelegate.dayData.summary.temp.toFixed(1) + "°C"; color: Style.colorSlate800 }
                                    }
                                    Rectangle {
                                        Layout.fillWidth: true; height: 48; color: "transparent"
                                        border.color: Style.colorTableBorder; border.width: 1
                                        Text { anchors.centerIn: parent; text: thDayDelegate.dayData.summary.humi.toFixed(1) + "%"; color: Style.colorSlate800 }
                                    }
                                    Rectangle {
                                        Layout.preferredWidth: 90; height: 48; color: "transparent"
                                        border.color: Style.colorTableBorder; border.width: 1
                                        Text { anchors.centerIn: parent; text: thDayDelegate.dayData.hours.length + "개 시간대"; color: Style.colorSlate500; font.pixelSize: 11 }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        var copy = Object.assign({}, expandedTempHumiDays);
                                        copy[thDayDelegate.dayKey] = !thDayDelegate.expanded;
                                        expandedTempHumiDays = copy;
                                    }
                                }
                            }

                            Repeater {
                                model: thDayDelegate.expanded ? thDayDelegate.dayData.hours : []
                                delegate: RowLayout {
                                    id: thHourRow
                                    property var hourData: modelData
                                    width: thDayDelegate.width; height: 40; spacing: 0

                                    Rectangle { Layout.preferredWidth: 90; height: 40; color: Style.colorSurface; border.color: Style.colorTableBorder
                                        Text { anchors.centerIn: parent; text: thHourRow.hourData.hour + "시"; color: Style.colorSlate800 } }
                                    Rectangle { Layout.fillWidth: true; height: 40; color: Style.colorSurface; border.color: Style.colorTableBorder
                                        Text { anchors.centerIn: parent; text: thHourRow.hourData.temp !== undefined ? thHourRow.hourData.temp.toFixed(1) + "°C" : "0.0°C"; color: Style.colorSlate800 } }
                                    Rectangle { Layout.fillWidth: true; height: 40; color: Style.colorSurface; border.color: Style.colorTableBorder
                                        Text { anchors.centerIn: parent; text: thHourRow.hourData.humi !== undefined ? thHourRow.hourData.humi.toFixed(1) + "%" : "0.0%"; color: Style.colorSlate800 } }
                                    Rectangle { Layout.preferredWidth: 90; height: 40; color: Style.colorSurface; border.color: Style.colorTableBorder }
                                }
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true; Layout.topMargin: 50
                        text: networkClient.isConnected ? "온습도 데이터가 없습니다." : "서버에 연결되지 않았습니다."
                        color: Style.colorSlate500; font: Style.fontNormal
                        horizontalAlignment: Text.AlignHCenter
                        visible: tempHumiStatsByDay.length === 0
                    }
                }
            }

            // ── Tab 2: Passenger Flow ───────────────────────────────────────
            ScrollView {
                contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width
                    spacing: 0

                    // Column header
                    RowLayout {
                        Layout.fillWidth: true; spacing: 0
                        Rectangle { Layout.preferredWidth: 90; height: 40; color: Style.colorTableHeadFlow; Text { anchors.centerIn: parent; text: "요일"; color: "white"; font.bold: true } }
                        Repeater {
                            model: 8
                            Rectangle { Layout.preferredWidth: 50; height: 40; color: Style.colorTableHeadFlow; Text { anchors.centerIn: parent; text: index + 1; color: "white"; font.bold: true } }
                        }
                        Rectangle { Layout.fillWidth: true; height: 40; color: Style.colorTableHeadFlow; Text { anchors.centerIn: parent; text: "전체 평균"; color: "white"; font.bold: true } }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: contentHeight
                        interactive: false
                        model: flowStatsByDay

                        delegate: Column {
                            id: flowDayDelegate
                            property var dayData: modelData
                            property string dayKey: String(dayData.day)
                            property bool expanded: expandedFlowDays.hasOwnProperty(dayKey) && expandedFlowDays[dayKey]
                            width: parent ? parent.width : 0

                            // Day summary row
                            Item {
                                width: parent.width; height: 52
                                Rectangle {
                                    anchors.fill: parent
                                    color: flowDayDelegate.expanded ? "#FEF9C3" : "#FEFCE8"
                                    border.color: Style.colorTableBorder; border.width: 1
                                }
                                RowLayout {
                                    anchors.fill: parent; spacing: 0
                                    Rectangle {
                                        Layout.preferredWidth: 90; height: 52; color: "transparent"
                                        Text {
                                            anchors.centerIn: parent
                                            text: (flowDayDelegate.expanded ? "▼  " : "▶  ") + getDayName(flowDayDelegate.dayData.day)
                                            font.bold: true; color: Style.colorSlate800
                                        }
                                    }
                                    Repeater {
                                        model: 8
                                        Rectangle {
                                            Layout.preferredWidth: 50; height: 52; color: "transparent"
                                            border.color: Style.colorTableBorder; border.width: 1
                                            Rectangle {
                                                anchors.fill: parent; anchors.margins: 3; radius: 3
                                                color: getCongestionColor(flowDayDelegate.dayData.summary.platforms[index])
                                                opacity: 0.9
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: Math.round(flowDayDelegate.dayData.summary.platforms[index])
                                                    color: "white"; font.bold: true; font.pixelSize: 12
                                                }
                                            }
                                        }
                                    }
                                    Rectangle {
                                        Layout.fillWidth: true; height: 52; color: "transparent"
                                        border.color: Style.colorTableBorder; border.width: 1
                                        Text {
                                            anchors.centerIn: parent
                                            text: Math.round(flowDayDelegate.dayData.summary.total / 8) + "명"
                                            color: Style.colorSlate800; font.bold: true
                                        }
                                    }
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        var copy = Object.assign({}, expandedFlowDays);
                                        copy[flowDayDelegate.dayKey] = !flowDayDelegate.expanded;
                                        expandedFlowDays = copy;
                                    }
                                }
                            }

                            // Hour rows (expanded)
                            Repeater {
                                model: flowDayDelegate.expanded ? flowDayDelegate.dayData.hours : []
                                delegate: Item {
                                    id: flowHourItem
                                    property var hourData: modelData
                                    width: flowDayDelegate.width; height: 40

                                    RowLayout {
                                        anchors.fill: parent; spacing: 0
                                        Rectangle {
                                            Layout.preferredWidth: 90; height: 40
                                            color: Style.colorSurface; border.color: Style.colorTableBorder
                                            Text { anchors.centerIn: parent; text: flowHourItem.hourData.hour + "시"; color: Style.colorSlate800 }
                                        }
                                        Repeater {
                                            model: 8
                                            Rectangle {
                                                Layout.preferredWidth: 50; height: 40
                                                color: Style.colorSurface; border.color: Style.colorTableBorder
                                                Rectangle {
                                                    anchors.fill: parent; anchors.margins: 1; radius: 2
                                                    color: getCongestionColor(flowHourItem.hourData.platforms[index])
                                                    opacity: 0.9
                                                    Text {
                                                        anchors.centerIn: parent
                                                        text: Math.round(flowHourItem.hourData.platforms[index])
                                                        color: "white"; font.bold: true; font.pixelSize: 11
                                                    }
                                                }
                                            }
                                        }
                                        Rectangle {
                                            Layout.fillWidth: true; height: 40
                                            color: Style.colorSurface; border.color: Style.colorTableBorder
                                            Text {
                                                anchors.centerIn: parent
                                                text: Math.round(flowHourItem.hourData.total / 8) + "명"
                                                color: Style.colorSlate800; font.bold: true
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true; Layout.topMargin: 50
                        text: networkClient.isConnected ? "분석된 승객 흐름 데이터가 없습니다.\n(데이터 수집 중이거나 서버에 정보가 부족할 수 있습니다)" : "서버에 연결되지 않았습니다."
                        color: Style.colorSlate500; font: Style.fontNormal
                        horizontalAlignment: Text.AlignHCenter
                        visible: flowStatsByDay.length === 0
                    }
                }
            }
        }

        BusyIndicator {
            anchors.centerIn: parent
            running: networkClient.isConnected && (airStatsByDay.length === 0 && flowStatsByDay.length === 0 && tempHumiStatsByDay.length === 0)
            z: 10
        }
    }
}
