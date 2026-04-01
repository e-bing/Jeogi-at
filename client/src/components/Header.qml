import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import ".."

Rectangle {
    id: headerRoot
    color: Style.colorSurface

    property string title: "통합 모니터링"

    // Bottom border
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Style.colorSlate200
    }

    property var currentTime: new Date()

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: headerRoot.currentTime = new Date()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 10
        anchors.topMargin: 10
        anchors.bottomMargin: 10

        Text {
            text: headerRoot.title
            font: Style.fontLarge
            color: Style.colorSlate800
            renderType: Text.QtRendering
        }

        // Tag
        Rectangle {
            width: 40
            height: 20
            radius: 4
            color: "#DCFCE7" // Light Green
            border.color: "#86EFAC"

            Text {
                anchors.centerIn: parent
                text: "LIVE"
                color: "#16A34A" // Green-600
                font.pixelSize: 10
                font.bold: true
            }
        }

        Item {
            Layout.fillWidth: true
        } // Spacer

        // Time
        RowLayout {
            spacing: -20 // Negative spacing to bring logo closer to text
            Image {
                source: "../logo/clock_logo.png"
                Layout.preferredWidth: 80
                Layout.preferredHeight: 80
                Layout.alignment: Qt.AlignVCenter
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
            }
            Text {
                text: Qt.formatDateTime(headerRoot.currentTime, "yyyy-MM-dd hh:mm:ss")
                color: Style.colorSlate500
                font.family: "Consolas"
                font.pixelSize: 13
                Layout.alignment: Qt.AlignVCenter
            }
        }


    }
}
