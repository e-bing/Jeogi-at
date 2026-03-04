import QtQuick 2.15
import QtQuick.Controls 2.15
import "../" // For Style singleton

Rectangle {
    color: Style.colorBackground

    Text {
        anchors.centerIn: parent
        text: "디바이스 제어 및 확인 페이지"
        font: Style.fontLarge
        color: Style.colorSlate800
        renderType: Text.QtRendering
    }
}
