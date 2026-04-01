pragma Singleton
import QtQuick 2.15

QtObject {
    // Theme state
    property bool isDarkMode: false

    // Colors
    property color colorPrimary: "#FF9F1C" // Orange-ish
    property color colorPrimaryDark: "#F78000"

    property color colorDanger: "#EF4444" // Red-500
    property color colorDangerDark: "#DC2626" // Red-600

    property color colorBackground: isDarkMode ? "#0F172A" : "#F8FAFC" // Slate-950 : Slate-50
    property color colorSurface: isDarkMode ? "#1E293B" : "#FFFFFF"    // Slate-800 : White

    property color colorSlate500: isDarkMode ? "#94A3B8" : "#64748B" // Slate-400 : Slate-500
    property color colorSlate600: isDarkMode ? "#CBD5E1" : "#475569" // Slate-300 : Slate-600
    property color colorSlate800: isDarkMode ? "#F8FAFC" : "#1E293B" // Slate-50 : Slate-800
    property color colorSlate200: isDarkMode ? "#334155" : "#E2E8F0" // Slate-700 : Slate-200
    property color colorSlate300: isDarkMode ? "#475569" : "#CBD5E1" // Slate-600 : Slate-300

    // Semantic Colors (Alerts)
    property color colorGreenAlert: isDarkMode ? "#064E3B" : "#F0FDF4"
    property color colorGreenAlertBorder: isDarkMode ? "#065F46" : "#BBF7D0"
    property color colorRedAlert: isDarkMode ? "#450A0A" : "#FEF2F2"
    property color colorRedAlertBorder: isDarkMode ? "#7F1D1D" : "#FECACA"
    property color colorYellowAlert: isDarkMode ? "#451A03" : "#FFFBEB"
    property color colorYellowAlertBorder: isDarkMode ? "#78350F" : "#FEF3C7"

    // Table Colors
    property color colorTableHeadCongestion: "#4CAF50"
    property color colorTableHeadAir: "#FF9800"
    property color colorTableHeadFlow: "#2196F3"
    property color colorTableBorder: isDarkMode ? "#334155" : "#EEEEEE"

    // Fonts
    property font fontLarge: Qt.font({
        family: "Pretendard",
        pointSize: 18,
        weight: Font.Bold
    })
    property font fontNormal: Qt.font({
        family: "Pretendard",
        pointSize: 10
    })
    property font fontRegular: Qt.font({
        family: "Pretendard",
        pointSize: 10
    })
    property font fontSmall: Qt.font({
        family: "Pretendard",
        pointSize: 9
    })
    property font fontBold: Qt.font({
        family: "Pretendard",
        pointSize: 10,
        weight: Font.Bold
    })
}
