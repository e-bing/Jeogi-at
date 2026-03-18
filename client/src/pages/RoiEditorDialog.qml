import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".."

Popup {
    id: roiEditor
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: parent.width * 0.92
    height: parent.height * 0.92
    modal: true
    closePolicy: Popup.NoAutoClose

    // ── 외부에서 설정하는 프로퍼티 ──────────────────────────────
    property int    cameraIndex:    0   // 0-3 (카메라 번호 - 1)
    property string cameraStringId: "" // "hanwha", "CAM_01" 등
    property var    workingZones:   [] // 편집 중인 존 사본
    property int    selectedZoneIdx: 0

    readonly property var zoneColors: ["#4488ff", "#44ff88", "#ffdd44", "#ff8844"]

    // 선택된 존의 ROI (핸들 위치 계산용)
    property var selRoi: {
        if (workingZones.length > selectedZoneIdx && workingZones[selectedZoneIdx])
            return workingZones[selectedZoneIdx].roi
        return [0, 0, 1, 1]
    }

    // ── 외부 호출 진입점 ─────────────────────────────────────────
    function openForCamera(camIdx, camStringId, zones) {
        cameraIndex    = camIdx
        cameraStringId = camStringId
        workingZones   = JSON.parse(JSON.stringify(zones))
        selectedZoneIdx = 0
        open()
    }

    // ── 배경 ─────────────────────────────────────────────────────
    background: Rectangle {
        color: "#1a1a2e"
        radius: 12
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // ── 제목 ─────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "ROI 설정  ·  CAM-0" + (roiEditor.cameraIndex + 1)
                      + (roiEditor.cameraStringId ? "  (" + roiEditor.cameraStringId + ")" : "")
                color: "white"
                font.bold: true
                font.pixelSize: 16
            }
            Item { Layout.fillWidth: true }
            Text {
                text: "꼭짓점을 드래그해서 구역을 조정하세요"
                color: "#94a3b8"
                font.pixelSize: 12
            }
        }

        // ── 메인 영역 ─────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            // ── 카메라 + ROI 오버레이 ─────────────────────────────
            Rectangle {
                id: camContainer
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#0f172a"
                radius: 8
                clip: true

                // 라이브 카메라 영상
                Image {
                    id: camImg
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectFit
                    cache: false
                    smooth: true
                    source: {
                        var id = roiEditor.cameraIndex + 1
                        if (id === 1) return dashboardRoot.cam1Source
                        if (id === 2) return dashboardRoot.cam2Source
                        if (id === 3) return dashboardRoot.cam3Source
                        if (id === 4) return dashboardRoot.cam4Source
                        return ""
                    }
                }

                // 신호 없을 때 안내 텍스트
                Text {
                    anchors.centerIn: parent
                    text: "카메라 신호 없음"
                    color: "#64748b"
                    font.pixelSize: 14
                    visible: camImg.status !== Image.Ready
                }

                // ── ROI 오버레이 (실제 이미지 렌더 영역에 정확히 맞춤) ──
                Item {
                    id: roiOverlay
                    // paintedWidth/Height: Image가 실제로 그린 영역 크기
                    x: (camContainer.width  - (camImg.paintedWidth  > 0 ? camImg.paintedWidth  : camContainer.width))  / 2
                    y: (camContainer.height - (camImg.paintedHeight > 0 ? camImg.paintedHeight : camContainer.height)) / 2
                    width:  camImg.paintedWidth  > 0 ? camImg.paintedWidth  : camContainer.width
                    height: camImg.paintedHeight > 0 ? camImg.paintedHeight : camContainer.height

                    // ── 존 사각형 (모든 존 표시) ─────────────────────
                    Repeater {
                        model: roiEditor.workingZones.length

                        Item {
                            property var    zone:    roiEditor.workingZones[index]
                            property bool   sel:     index === roiEditor.selectedZoneIdx
                            property string zColor:  roiEditor.zoneColors[index % 4]

                            // 사각형
                            Rectangle {
                                x:      zone.roi[0] * roiOverlay.width
                                y:      zone.roi[1] * roiOverlay.height
                                width:  (zone.roi[2] - zone.roi[0]) * roiOverlay.width
                                height: (zone.roi[3] - zone.roi[1]) * roiOverlay.height
                                color:  sel ? Qt.rgba(0.27, 0.53, 1, 0.25) : Qt.rgba(0, 0, 0, 0.1)
                                border.color: zColor
                                border.width: sel ? 2 : 1

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: roiEditor.selectedZoneIdx = index
                                }
                            }

                            // 존 레이블
                            Text {
                                x: zone.roi[0] * roiOverlay.width + 6
                                y: zone.roi[1] * roiOverlay.height + 4
                                text: "Zone " + zone.zone_id
                                color: "white"
                                font.pixelSize: 11
                                font.bold: true
                                style: Text.Outline
                                styleColor: "black"
                            }
                        }
                    }

                    // ── 선택된 존의 4개 꼭짓점 핸들 ─────────────────
                    // corner: 0=TL, 1=TR, 2=BR, 3=BL
                    Repeater {
                        model: roiEditor.workingZones.length > 0 ? 4 : 0

                        Item {
                            id: handle
                            z: 10
                            width: 18
                            height: 18

                            property int  corner: index
                            // corner별 정규화 좌표
                            property real normX: (corner === 0 || corner === 3)
                                                 ? roiEditor.selRoi[0] : roiEditor.selRoi[2]
                            property real normY: (corner === 0 || corner === 1)
                                                 ? roiEditor.selRoi[1] : roiEditor.selRoi[3]

                            // 드래그 중이 아닐 때만 ROI 좌표에 바인딩
                            Binding {
                                target:   handle
                                property: "x"
                                value:    handle.normX * roiOverlay.width - 9
                                when:     !hDrag.drag.active
                            }
                            Binding {
                                target:   handle
                                property: "y"
                                value:    handle.normY * roiOverlay.height - 9
                                when:     !hDrag.drag.active
                            }

                            // 핸들 원형 버튼
                            Rectangle {
                                anchors.fill: parent
                                radius: 9
                                color: "white"
                                border.color: roiEditor.zoneColors[roiEditor.selectedZoneIdx % 4]
                                border.width: 2

                                // 모서리별 내부 색
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 6; height: 6; radius: 3
                                    color: roiEditor.zoneColors[roiEditor.selectedZoneIdx % 4]
                                }
                            }

                            MouseArea {
                                id: hDrag
                                anchors.fill: parent
                                drag.target: handle
                                drag.minimumX: -9
                                drag.maximumX: roiOverlay.width  - 9
                                drag.minimumY: -9
                                drag.maximumY: roiOverlay.height - 9
                                cursorShape: Qt.CrossCursor

                                onPositionChanged: {
                                    if (!drag.active) return
                                    // 핸들 중심 → 정규화 좌표
                                    var nx = Math.max(0, Math.min(1, (handle.x + 9) / roiOverlay.width))
                                    var ny = Math.max(0, Math.min(1, (handle.y + 9) / roiOverlay.height))

                                    // workingZones 업데이트 (깊은 복사)
                                    var wz  = roiEditor.workingZones.slice()
                                    var z   = JSON.parse(JSON.stringify(wz[roiEditor.selectedZoneIdx]))
                                    var roi = z.roi.slice()

                                    if      (corner === 0) { roi[0] = nx; roi[1] = ny }
                                    else if (corner === 1) { roi[2] = nx; roi[1] = ny }
                                    else if (corner === 2) { roi[2] = nx; roi[3] = ny }
                                    else                   { roi[0] = nx; roi[3] = ny }

                                    // 최소 크기 보정 (5%)
                                    if (roi[2] - roi[0] < 0.05) {
                                        if (corner === 0 || corner === 3) roi[0] = roi[2] - 0.05
                                        else                               roi[2] = roi[0] + 0.05
                                    }
                                    if (roi[3] - roi[1] < 0.05) {
                                        if (corner === 0 || corner === 1) roi[1] = roi[3] - 0.05
                                        else                               roi[3] = roi[1] + 0.05
                                    }

                                    z.roi = roi
                                    wz[roiEditor.selectedZoneIdx] = z
                                    roiEditor.workingZones = wz
                                }
                            }
                        }
                    }
                }
            }

            // ── 오른쪽 패널 (존 목록 + 좌표 표시) ───────────────
            Rectangle {
                Layout.preferredWidth: 170
                Layout.fillHeight: true
                color: "#0f172a"
                radius: 8

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Text {
                        text: "구역 선택"
                        color: "white"
                        font.bold: true
                        font.pixelSize: 13
                    }

                    Repeater {
                        model: roiEditor.workingZones.length

                        Rectangle {
                            Layout.fillWidth: true
                            height: 40
                            radius: 6
                            color: (index === roiEditor.selectedZoneIdx)
                                   ? roiEditor.zoneColors[index % 4] : "#1e293b"
                            border.color: roiEditor.zoneColors[index % 4]
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "Zone " + roiEditor.workingZones[index].zone_id
                                color: "white"
                                font.pixelSize: 12
                                font.bold: index === roiEditor.selectedZoneIdx
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: roiEditor.selectedZoneIdx = index
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    // 좌표 수치 표시
                    Rectangle {
                        Layout.fillWidth: true
                        height: 90
                        color: "#1e293b"
                        radius: 6
                        visible: roiEditor.workingZones.length > 0

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4

                            Text {
                                text: "TL  (" + (roiEditor.selRoi[0] * 100).toFixed(1)
                                      + "%, " + (roiEditor.selRoi[1] * 100).toFixed(1) + "%)"
                                color: "#94a3b8"; font.pixelSize: 11
                            }
                            Text {
                                text: "BR  (" + (roiEditor.selRoi[2] * 100).toFixed(1)
                                      + "%, " + (roiEditor.selRoi[3] * 100).toFixed(1) + "%)"
                                color: "#94a3b8"; font.pixelSize: 11
                            }
                            Text {
                                text: "W " + ((roiEditor.selRoi[2] - roiEditor.selRoi[0]) * 100).toFixed(1)
                                      + "%  H " + ((roiEditor.selRoi[3] - roiEditor.selRoi[1]) * 100).toFixed(1) + "%"
                                color: "#64748b"; font.pixelSize: 10
                            }
                        }
                    }
                }
            }
        }

        // ── 하단 버튼 ─────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Item { Layout.fillWidth: true }

            // 취소
            Rectangle {
                width: 110; height: 42; radius: 8
                color: cancelMa.containsMouse ? "#334155" : "#1e293b"
                border.color: "#475569"; border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "취소"
                    color: "white"; font.pixelSize: 14
                }
                MouseArea {
                    id: cancelMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: roiEditor.close()
                }
            }

            // 확인 → 서버로 전송
            Rectangle {
                width: 110; height: 42; radius: 8
                color: confirmMa.pressed ? "#1d4ed8"
                       : confirmMa.containsMouse ? "#3b82f6" : "#2563eb"

                Text {
                    anchors.centerIn: parent
                    text: "확인"
                    color: "white"; font.pixelSize: 14; font.bold: true
                }
                MouseArea {
                    id: confirmMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        for (var i = 0; i < roiEditor.workingZones.length; i++) {
                            var z = roiEditor.workingZones[i]
                            networkClient.sendRoiUpdate(z.camera_id, z.zone_id, z.roi)
                        }
                        roiEditor.close()
                    }
                }
            }
        }
    }
}
