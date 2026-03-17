import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import ".." // To access Style singleton

Rectangle {
    color: Style.colorSurface

    // Navigation Signal
    signal pageSelected(string source, string title)
    property int currentIndex: 0

    // Right border
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Style.colorSlate200
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        // Logo Area
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 100 // Matches Header height (100) for alignment
            color: "transparent"

            RowLayout {
                anchors.centerIn: parent
                spacing: 12

                Image {
                    source: "../logo/logo.png"
                    Layout.preferredWidth: 160
                    Layout.preferredHeight: 60
                    fillMode: Image.PreserveAspectFit
                }
            }
        }

        // Navigation Items
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Repeater {
                model: [
                    {
                        name: "1. 통합 모니터링",
                        title: "통합 모니터링",
                        icon: "../logo/camera_logo.png",
                        reverseIcon: "../logo/reverse_camera_logo.png",
                        iconSize: 50,
                        darkIconSize: 80,
                        pageSource: "pages/Dashboard.qml"
                    },
                    {
                        name: "2. 통계 분석",
                        title: "통계 분석",
                        icon: "../logo/chart_logo.png",
                        reverseIcon: "../logo/reverse_chart_logo.png",
                        iconSize: 50,
                        darkIconSize: 80,
                        pageSource: "pages/Statistics.qml"
                    },
                    {
                        name: "3. 디바이스 확인",
                        title: "디바이스 확인",
                        icon: "../logo/device_logo.png",
                        reverseIcon: "../logo/reverse_device_logo.png",
                        iconSize: 50,
                        darkIconSize: 60,
                        pageSource: "pages/CameraManagement.qml"
                    },
                    {
                        name: "4. 녹화 관리",
                        title: "녹화 관리",
                        icon: "../logo/manual_logo.png",
                        reverseIcon: "",
                        iconSize: 50,
                        darkIconSize: 60,
                        pageSource: "pages/RecordingPage.qml"
                    }
                ]

                delegate: Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60 // Fixed height for a compact menu
                    color: index === currentIndex ? Style.colorPrimary : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        spacing: 12

                        // Icon Placeholder
                        Item {
                            width: 50 // Fixed width for alignment
                            height: parent.height

                            // Image Icon (if available)
                            Image {
                                anchors.centerIn: parent
                                width: Style.isDarkMode ? modelData.darkIconSize : modelData.iconSize
                                height: Style.isDarkMode ? modelData.darkIconSize : modelData.iconSize
                                source: Style.isDarkMode && modelData.reverseIcon ? modelData.reverseIcon : (modelData.icon ? modelData.icon : "")
                                visible: (Style.isDarkMode && modelData.reverseIcon) || modelData.icon ? true : false
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: true
                            }

                            // Fallback Shape (if no icon)
                            Rectangle {
                                anchors.centerIn: parent
                                width: modelData.iconSize * 0.7
                                height: modelData.iconSize * 0.7
                                color: index === currentIndex ? "white" : Style.colorSlate500
                                radius: 4
                                visible: !modelData.icon
                            }
                        }

                        Text {
                            text: modelData.name
                            color: index === currentIndex ? "white" : Style.colorSlate500
                            font: Style.fontNormal
                            renderType: Text.QtRendering
                            Layout.fillWidth: true
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            currentIndex = index;
                            pageSelected(modelData.pageSource, modelData.title);
                            console.log("Clicked " + modelData.name + ", Loading: " + modelData.pageSource);
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillHeight: true
        } // Spacer

        // Bottom/Dark mode toggle
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: "transparent"

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: Style.isDarkMode = !Style.isDarkMode
            }

            RowLayout {
                anchors.centerIn: parent
                spacing: 12

                Text {
                    text: Style.isDarkMode ? "☀️" : "🌙"
                    font.pixelSize: 18
                }

                Text {
                    text: Style.isDarkMode ? "라이트 모드로 전환" : "다크 모드로 전환"
                    color: Style.colorSlate600
                    font: Style.fontNormal
                }
            }
        }
    }
}
