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

    // Global server settings (editable via header)
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
}
