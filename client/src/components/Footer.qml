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

// Broadcast Button
            // MouseArea {
            //     implicitWidth: rowBroadcast.width
            //     implicitHeight: rowBroadcast.height
            //     cursorShape: Qt.PointingHandCursor

            //     RowLayout {
            //         id: rowBroadcast
            //         spacing: 5
            //         Rectangle {
            //             width: 8
            //             height: 8
            //             radius: 4
            //             color: Style.colorPrimary
            //         }
            //         Text {
            //             text: "안내 방송"
            //             font: Style.fontBold
            //             color: Style.colorSlate500
            //         }
            //     }
            // }
            // test: mic streamer
            RowLayout {
                spacing: 5

                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: broadcastText.broadcasting ? "#ff4444" : Style.colorPrimary
                }

                Text {
                    id: broadcastText

                    property bool broadcasting: false

                    text: broadcasting ? "방송 중..." : "안내 방송"
                    font: Style.fontBold
                    color: broadcasting ? "#ff4444" : Style.colorSlate500

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor

                        onClicked: {
                            if (broadcastText.broadcasting) {
                                micStreamer.stopStreaming()
                                broadcastText.broadcasting = false
                            } else {
                                const ok = micStreamer.startStreaming()
                                if (ok)
                                    broadcastText.broadcasting = true
                            }
                        }
                    }
                }
            }
            //
        }

        Item {
            Layout.fillWidth: true
        } // Spacer

        // System Info
        RowLayout {
            spacing: 15

            // Connection status
            Text {
                text: networkClient.statusMessage
                color: Style.colorSlate500
                font.pixelSize: 11
                width: 160
                elide: Text.ElideRight
            }
            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: networkClient.isConnected ? "#22C55E" : "#EF4444"
            }
            Button {
                text: networkClient.isConnected ? "Disconnect" : "Connect"
                implicitWidth: 90
                implicitHeight: 28
                font.pixelSize: 11
                onClicked: {
                    if (networkClient.isConnected)
                        networkClient.disconnectFromServer();
                    else
                        networkClient.connectToServer(mainWindow.serverIp, mainWindow.serverPort);
                }
            }

            // Server settings (gear)
            Rectangle {
                width: 24
                height: 24
                color: Style.colorSlate200
                radius: 12
                Text {
                    anchors.centerIn: parent
                    text: "⚙️"
                    font.pixelSize: 12
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: footerServerDialog.open()
                }
            }

            Dialog {
                id: footerServerDialog
                title: "서버 설정"
                standardButtons: Dialog.Ok | Dialog.Cancel
                modal: true
                anchors.centerIn: Overlay.overlay
                contentItem: ColumnLayout {
                    spacing: 8
                    width: 320
                    TextField {
                        id: footerIpField
                        placeholderText: "Server IP"
                        text: mainWindow.serverIp
                        Keys.onReturnPressed: footerServerDialog.accept()
                    }
                    TextField {
                        id: footerPortField
                        placeholderText: "Server Port"
                        text: mainWindow.serverPort.toString()
                        inputMethodHints: Qt.ImhDigitsOnly
                        Keys.onReturnPressed: footerServerDialog.accept()
                    }
                }
                onAccepted: {
                    if (footerIpField.text && footerIpField.text.length > 0)
                        mainWindow.serverIp = footerIpField.text;
                    var p = parseInt(footerPortField.text);
                    if (!isNaN(p))
                        mainWindow.serverPort = p;
                }
            }

            Rectangle {
                width: 1
                height: 12
                color: Style.colorSlate300
            }
            Text {
                text: "IP: " + (networkClient.isConnected && mainWindow.serverIp.length > 0 ? mainWindow.serverIp : "-")
                color: Style.colorSlate500
                font.pixelSize: 11
                font.family: "Consolas"
                Layout.minimumWidth: 130
                Layout.preferredWidth: 130
            }
            Text {
                text: "© VEDADEV"
                color: Style.colorSlate300
                font.pixelSize: 11
            }
        }
    }
}
