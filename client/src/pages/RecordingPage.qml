import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import ".."

Rectangle {
    id: recordingRoot
    color: Style.colorBackground

    property int selectedCamera: 1
    property var recordings: []
    property bool recActive: false
    property bool manualStopped: false
    property string errorMessage: ""
    property var camRecordingStates: [false, false, false, false]

    function refresh() {
        recordings = recordingManager.getRecordings(selectedCamera)
        recActive  = recordingManager.isRecording(selectedCamera)
    }

    Component.onCompleted: {
        refresh()
        var states = [false, false, false, false]
        for (var i = 1; i <= 4; i++)
            states[i - 1] = recordingManager.isRecording(i)
        camRecordingStates = states
    }

    Connections {
        target: recordingManager
        function onRecordingStateChanged(cameraId, recording) {
            var states = recordingRoot.camRecordingStates.slice()
            states[cameraId - 1] = recording
            recordingRoot.camRecordingStates = states
            if (cameraId === recordingRoot.selectedCamera) {
                recordingRoot.recActive = recording
                recordingRoot.refresh()
                if (!recording)
                    durationRefreshTimer.restart()
            }
        }
        function onRecordingsChanged(cameraId) {
            if (cameraId === selectedCamera) refresh()
        }
        function onRecordingError(cameraId, message) {
            errorMessage = message
            errorTimer.restart()
        }
    }

    Timer {
        id: durationRefreshTimer
        interval: 800
        repeat: false
        onTriggered: recordingRoot.refresh()
    }

    Timer {
        id: errorTimer
        interval: 5000
        repeat: false
        onTriggered: errorMessage = ""
    }

    // 에러 배너
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 20
        height: visible ? 48 : 0
        radius: 8
        color: "#FEF2F2"
        border.color: "#FECACA"
        border.width: 1
        visible: errorMessage !== ""
        z: 10

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 10

            Text {
                text: "⚠️"
                font.pixelSize: 16
            }
            Text {
                text: errorMessage
                color: "#DC2626"
                font.pixelSize: 12
                font.family: "Pretendard"
                renderType: Text.QtRendering
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            Rectangle {
                width: 20; height: 20; radius: 4
                color: closeArea.containsMouse ? "#FECACA" : "transparent"
                Text { anchors.centerIn: parent; text: "✕"; color: "#DC2626"; font.pixelSize: 11 }
                MouseArea {
                    id: closeArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: errorMessage = ""
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        // ── 왼쪽: 카메라 선택 패널 ──────────────────────────────
        Rectangle {
            Layout.preferredWidth: 180
            Layout.fillHeight: true
            color: Style.colorSurface
            radius: 12

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10

                Text {
                    text: "카메라 선택"
                    font: Style.fontBold
                    color: Style.colorSlate800
                    renderType: Text.QtRendering
                }

                Repeater {
                    model: [1, 2, 3, 4]
                    delegate: Rectangle {
                        id: camDelegate
                        required property int modelData
                        Layout.fillWidth: true
                        height: 56
                        radius: 8
                        color: recordingRoot.selectedCamera === camDelegate.modelData
                               ? Style.colorPrimary
                               : (camArea.containsMouse ? Style.colorSlate200 : "transparent")

                        Behavior on color { ColorAnimation { duration: 120 } }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 10

                            // 녹화 중 표시 점
                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                color: recordingRoot.camRecordingStates[camDelegate.modelData - 1]
                                       ? Style.colorDanger : Style.colorSlate300
                                SequentialAnimation on opacity {
                                    running: recordingRoot.camRecordingStates[camDelegate.modelData - 1]
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 0.2; duration: 500 }
                                    NumberAnimation { to: 1.0; duration: 500 }
                                }
                            }

                            Text {
                                text: "CAM " + camDelegate.modelData
                                color: recordingRoot.selectedCamera === camDelegate.modelData ? "white" : Style.colorSlate600
                                font: Style.fontNormal
                                renderType: Text.QtRendering
                                Layout.fillWidth: true
                            }
                        }

                        MouseArea {
                            id: camArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                recordingRoot.selectedCamera = camDelegate.modelData
                                recordingRoot.refresh()
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        // ── 오른쪽: 녹화 제어 + 목록 ────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16

            // 녹화 제어 바
            Rectangle {
                Layout.fillWidth: true
                height: 72
                color: Style.colorSurface
                radius: 12

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    spacing: 16

                    Text {
                        text: "CAM " + selectedCamera + " 녹화"
                        font: Style.fontBold
                        color: Style.colorSlate800
                        renderType: Text.QtRendering
                    }

                    Item { Layout.fillWidth: true }

                    // REC 점멸 인디케이터
                    Row {
                        spacing: 6
                        visible: recActive

                        Rectangle {
                            width: 10
                            height: 10
                            radius: 5
                            color: Style.colorDanger
                            anchors.verticalCenter: parent.verticalCenter
                            SequentialAnimation on opacity {
                                running: recActive
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.1; duration: 600 }
                                NumberAnimation { to: 1.0; duration: 600 }
                            }
                        }

                        Text {
                            text: "REC"
                            color: Style.colorDanger
                            font: Style.fontBold
                            renderType: Text.QtRendering
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    // 녹화 시작 / 중지 버튼
                    Rectangle {
                        width: 130
                        height: 40
                        radius: 8
                        color: recActive ? Style.colorDanger : Style.colorPrimary

                        Behavior on color { ColorAnimation { duration: 150 } }

                        Row {
                            anchors.centerIn: parent
                            spacing: 6

                            Text {
                                text: recActive ? "■" : "●"
                                color: "white"
                                font.pixelSize: 14
                                renderType: Text.QtRendering
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: recActive ? "녹화 중지" : (manualStopped ? "녹화 재시작" : "녹화 시작")
                                color: "white"
                                font: Style.fontNormal
                                renderType: Text.QtRendering
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (recActive) {
                                    manualStopped = true
                                    recordingManager.stopRecording(selectedCamera)
                                } else {
                                    manualStopped = false
                                    recordingManager.startRecording(selectedCamera)
                                }
                            }
                        }
                    }
                }
            }

            // 녹화 목록 패널
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Style.colorSurface
                radius: 12

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: "저장된 녹화 목록"
                            font: Style.fontBold
                            color: Style.colorSlate800
                            renderType: Text.QtRendering
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: "총 " + recordings.length + "개"
                            font: Style.fontNormal
                            color: Style.colorSlate500
                            renderType: Text.QtRendering
                        }
                    }

                    // 빈 목록 안내
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: recordings.length === 0

                        Column {
                            anchors.centerIn: parent
                            spacing: 12

                            Text {
                                text: "🎥"
                                font.pixelSize: 40
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                            Text {
                                text: "저장된 녹화가 없습니다."
                                color: Style.colorSlate500
                                font: Style.fontNormal
                                renderType: Text.QtRendering
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                            Text {
                                text: "상단의 '녹화 시작' 버튼을 눌러 녹화하세요."
                                color: Style.colorSlate300
                                font: Style.fontSmall
                                renderType: Text.QtRendering
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }
                    }

                    // 녹화 그리드
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        visible: recordings.length > 0

                        GridView {
                            width: parent.width
                            model: recordings
                            cellWidth: 220
                            cellHeight: 210
                            clip: true

                            delegate: Item {
                                width: 210
                                height: 200

                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: 5
                                    color: Style.colorBackground
                                    radius: 10
                                    border.color: Style.colorSlate200
                                    border.width: 1

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 6

                                        // 썸네일
                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 115
                                            color: "#1E293B"
                                            radius: 6
                                            clip: true

                                            Image {
                                                anchors.fill: parent
                                                source: modelData.thumbPath || ""
                                                fillMode: Image.PreserveAspectCrop
                                                smooth: true
                                                onStatusChanged: {
                                                    if (status === Image.Error)
                                                        source = ""
                                                }
                                            }

                                            Text {
                                                anchors.centerIn: parent
                                                text: "CAM " + selectedCamera
                                                color: Style.colorSlate300
                                                font: Style.fontSmall
                                                renderType: Text.QtRendering
                                                visible: !modelData.thumbPath
                                            }
                                        }

                                        // 세션 이름 (날짜시간)
                                        Text {
                                            text: modelData.name || ""
                                            color: Style.colorSlate800
                                            font.pixelSize: 11
                                            font.family: "Pretendard"
                                            renderType: Text.QtRendering
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        // 길이 / 재생 / 삭제
                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 4

                                            Text {
                                                text: modelData.duration || "--:--"
                                                color: Style.colorSlate500
                                                font.pixelSize: 11
                                                font.family: "Pretendard"
                                                renderType: Text.QtRendering
                                            }

                                            Item { Layout.fillWidth: true }

                                            // 재생 버튼
                                            Rectangle {
                                                width: 24
                                                height: 24
                                                radius: 4
                                                color: playArea.containsMouse ? "#DBEAFE" : "transparent"

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "▶"
                                                    color: Style.colorPrimary
                                                    font.pixelSize: 11
                                                    renderType: Text.QtRendering
                                                }

                                                MouseArea {
                                                    id: playArea
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: Qt.openUrlExternally(modelData.videoPath)
                                                }
                                            }

                                            // 삭제 버튼
                                            Rectangle {
                                                width: 24
                                                height: 24
                                                radius: 4
                                                color: delArea.containsMouse ? "#FEE2E2" : "transparent"

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "✕"
                                                    color: Style.colorDanger
                                                    font.pixelSize: 12
                                                    renderType: Text.QtRendering
                                                }

                                                MouseArea {
                                                    id: delArea
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        recordingManager.deleteRecording(modelData.path)
                                                        refresh()
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
