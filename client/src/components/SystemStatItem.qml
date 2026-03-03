import QtQuick 2.15
import QtQuick.Layouts 1.15
import ".."

ColumnLayout {
    id: root
    property string title: ""
    property real value: 0
    property string unit: ""
    property color color: "#0EA5E9"
    property real maxValue: 100

    Layout.fillWidth: true
    spacing: 8

    RowLayout {
        Layout.fillWidth: true
        Text {
            text: root.title
            font: Style.fontBold
            font.pixelSize: 13
            color: Style.colorSlate600
        }
        Item {
            Layout.fillWidth: true
        }
        Text {
            text: root.value.toFixed(1) + root.unit
            font: Style.fontBold
            font.pixelSize: 14
            color: Style.colorSlate800
        }
    }

    Rectangle {
        Layout.fillWidth: true
        height: 10
        radius: 5
        color: Style.isDarkMode ? "#334155" : "#F1F5F9"
        clip: true

        Rectangle {
            id: bar
            height: parent.height
            radius: 5
            width: isNaN(root.value / root.maxValue) ? 0 : Math.min(parent.width, (root.value / root.maxValue) * parent.width)
            color: root.color

            Behavior on width {
                NumberAnimation {
                    duration: 1000
                    easing.type: Easing.OutCubic
                }
            }

            // Highlight effect
            Rectangle {
                anchors.fill: parent
                radius: 5
                opacity: 0.2
                gradient: Gradient {
                    GradientStop {
                        position: 0.0
                        color: "#44FFFFFF"
                    }
                    GradientStop {
                        position: 1.0
                        color: "transparent"
                    }
                }
            }
        }
    }
}
