import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "."

Window {
    id: mainWindow
    width: 1280
    height: 800
    visible: true
    title: qsTr("Smart Subway Management System")
    color: Style.colorBackground

    // Global server settings (editable via header)
    property string serverIp: ""
    property int serverPort: 12345

    // 앱 시작 시 1초 후 자동 접속 (serverIp 설정 후 재접속도 처리)
    Timer {
        id: autoConnectTimer
        interval: 1000
        repeat: false
        running: true
        onTriggered: {
            if (mainWindow.serverIp !== "")
                networkClient.connectToServer(mainWindow.serverIp, mainWindow.serverPort)
        }
    }

    onServerIpChanged: {
        if (serverIp !== "" && !networkClient.isConnected)
            networkClient.connectToServer(serverIp, serverPort)
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Sidebar
        Loader {
            id: sidebarLoader
            Layout.fillHeight: true
            Layout.preferredWidth: 350 // Increased to 350 for extra large logo
            source: "components/Sidebar.qml"

            Connections {
                target: sidebarLoader.item
                function onPageSelected(pageSource, pageTitle) {
                    pageLoader.source = pageSource;
                    if (headerLoader.item) {
                        headerLoader.item.title = pageTitle;
                    }
                }
            }
        }

        // Main Content Area
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Header
            Loader {
                id: headerLoader
                Layout.fillWidth: true
                Layout.preferredHeight: 100 // Increased from 80
                source: "components/Header.qml"
            }

            // Page Content
            Loader {
                id: pageLoader
                Layout.fillWidth: true
                Layout.fillHeight: true
                source: "pages/Dashboard.qml" // Default page

                onStatusChanged: {
                    if (status === Loader.Error) {
                        console.error("Loader Error: Failed to load " + source);
                    }
                }
                onLoaded: {
                    console.log("Successfully loaded page: " + source);
                }
            }

            // Footer
            Loader {
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                source: "components/Footer.qml"
            }
        }
    }
}
