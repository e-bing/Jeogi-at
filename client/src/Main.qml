import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import com.metro.network 1.0
import "."

Window {
    id: mainWindow
    width: 1280
    height: 800
    visible: true
    title: qsTr("Smart Subway Management System")
    color: Style.colorBackground

    // Global server settings (change here to affect all pages)
    // 팝업으로 편집 가능
    property string serverIp: "192.168.0.48"
    property int serverPort: 12345

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

                // padding around content

            }

            // Footer
            Loader {
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                source: "components/Footer.qml"
            }
        }
    }

    // Floating server settings button + dialog
    Button {
        id: serverSettingsButton
        text: "서버 설정"
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 12
        onClicked: serverDialog.open()
    }

    Dialog {
        id: serverDialog
        title: "서버 설정"
        standardButtons: Dialog.Ok | Dialog.Cancel
        modal: true
        contentItem: ColumnLayout {
            spacing: 8
            width: 360
            TextField {
                id: ipField
                placeholderText: "Server IP"
                text: mainWindow.serverIp
            }
            TextField {
                id: portField
                placeholderText: "Server Port"
                text: mainWindow.serverPort.toString()
                inputMethodHints: Qt.ImhDigitsOnly
            }
        }
        onAccepted: {
            // Apply changes globally
            if (ipField.text && ipField.text.length > 0)
                mainWindow.serverIp = ipField.text;
            var p = parseInt(portField.text);
            if (!isNaN(p))
                mainWindow.serverPort = p;
        }
    }
}
