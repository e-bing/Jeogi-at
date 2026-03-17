import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import ".."

Rectangle {
    color: Style.colorSurface

    // Top border
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Style.colorSlate200
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20

        // Emergency Controls
        RowLayout {
            spacing: 15

            // Siren Button
            MouseArea {
                implicitWidth: rowSiren.width
                implicitHeight: rowSiren.height
                cursorShape: Qt.PointingHandCursor

                RowLayout {
                    id: rowSiren
                    spacing: 5
                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: Style.colorDanger
                    }
                    Text {
                        text: "비상 사이렌"
                        font: Style.fontBold
                        color: Style.colorSlate500
                    }
                }
            }

            // Broadcast Button
            MouseArea {
                implicitWidth: rowBroadcast.width
                implicitHeight: rowBroadcast.height
                cursorShape: Qt.PointingHandCursor

                RowLayout {
                    id: rowBroadcast
                    spacing: 5
                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: Style.colorPrimary
                    }
                    Text {
                        text: "안내 방송"
                        font: Style.fontBold
                        color: Style.colorSlate500
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
        } // Spacer

        // System Info
        RowLayout {
            spacing: 15

            // Connection status
            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: networkClient.isConnected ? "#22C55E" : "#EF4444"
            }
            Text {
                text: networkClient.statusMessage
                color: Style.colorSlate500
                font.pixelSize: 11
            }
            Button {
                text: networkClient.isConnected ? "Disconnect" : "Connect"
                implicitHeight: 28
                font.pixelSize: 11
                onClicked: {
                    if (networkClient.isConnected)
                        networkClient.disconnectFromServer();
                    else
                        networkClient.connectToServer(mainWindow.serverIp, mainWindow.serverPort);
                }
            }

            Rectangle {
                width: 1
                height: 12
                color: Style.colorSlate300
            }
            Text {
                text: "관리자: 조예찬"
                color: Style.colorSlate500
                font.pixelSize: 11
            }
            Rectangle {
                width: 1
                height: 12
                color: Style.colorSlate300
            }
            Text {
                text: "IP: " + (mainWindow.serverIp.length > 0 ? mainWindow.serverIp : "-")
                color: Style.colorSlate500
                font.pixelSize: 11
                font.family: "Consolas"
            }
            Text {
                text: "© VEDADEV"
                color: Style.colorSlate300
                font.pixelSize: 11
            }
        }
    }
}
