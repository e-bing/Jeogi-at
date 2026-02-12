import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import com.metro.network 1.0
import ".."

ColumnLayout {
    width: parent.width
    height: parent.height
    spacing: 20

    property var realtimeData: []

    Timer {
        id: connectionTimer
        interval: 1000
        repeat: false
        onTriggered: {
            console.log("CameraManagement - Auto-connecting...");
            client.connectToServer("aboy.local", 12345);
        }
    }

    NetworkClient {
        id: client
        onIsConnectedChanged: {
            console.log("CameraManagement - Connected: " + client.isConnected);
        }
        onStatusMessageChanged: {
            console.log("CameraManagement - Status: " + client.statusMessage);
        }
        onRealtimeDataReceived: function (data) {
            console.log("CameraManagement - Realtime Data Received: " + data.length);
            realtimeData = data;
        }
        Component.onCompleted: {
            console.log("CameraManagement - Page ready, scheduling connection...");
            connectionTimer.start();
        }
        Component.onDestruction: {
            console.log("CameraManagement - Disconnecting on destruction...");
            disconnectFromServer();
        }
    }

    // Header: Title + Status
    RowLayout {
        Layout.fillWidth: true
        Layout.margins: 20
        spacing: 20

        Text {
            text: "디바이스 제어 및 확인"
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
                    client.connectToServer("aboy.local", 12345);
            }
        }
    }

    // Content Area
    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: Style.colorSurface
        radius: 12
        border.color: Style.colorSlate200
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 15

            Text {
                text: "카메라 및 센서 관리"
                font: Style.fontLarge
                color: Style.colorSlate800
            }

            Text {
                text: "실시간 데이터: " + realtimeData.length + " 항목"
                color: Style.colorSlate600
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }
}
