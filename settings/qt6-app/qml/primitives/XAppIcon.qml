import QtQuick
import XType.Settings 1.0

Rectangle {
    id: root
    property string cls: ""
    // Theme icon name or path from the app's .desktop file; shown when it resolves.
    property string iconName: ""
    // Letter shown on the badge when there is no cls and no icon.
    property string fallbackLetter: ""

    width: 32; height: 32; radius: 7
    readonly property bool _hasIcon: themeIcon.status === Image.Ready
    border.width: 1; border.color: cls === "" && !_hasIcon ? Theme.ink12 : "transparent"
    color: "transparent"

    readonly property color _g0: {
        switch (cls) {
            case "kate": return "#5d8aa8"
            case "tb":   return "#4a90d9"
            case "kons": return "#2a2a2a"
            case "fox":  return "#ff7139"
            case "disc": return "#5865f2"
            case "kde":  return Theme.purpleSoft
            default:     return Theme.ink12
        }
    }
    readonly property color _g1: {
        switch (cls) {
            case "kate": return "#3a5d77"
            case "tb":   return "#2a5a9a"
            case "kons": return "#555555"
            case "fox":  return "#c75200"
            case "disc": return "#3a4ad9"
            case "kde":  return Theme.purpleDeep
            default:     return Theme.ink06
        }
    }

    gradient: _hasIcon ? null : badgeGradient

    Gradient {
        id: badgeGradient
        orientation: Gradient.Horizontal
        GradientStop { position: 0.0; color: root._g0 }
        GradientStop { position: 1.0; color: root._g1 }
    }

    Image {
        id: themeIcon
        anchors.fill: parent
        source: root.iconName !== "" ? "image://appicon/" + root.iconName : ""
        sourceSize: Qt.size(64, 64)
        fillMode: Image.PreserveAspectFit
        smooth: true
        visible: root._hasIcon
    }

    Text {
        anchors.centerIn: parent
        visible: !root._hasIcon
        text: root.cls !== "" ? root.cls.slice(0, 1).toUpperCase()
            : root.fallbackLetter !== "" ? root.fallbackLetter.slice(0, 1).toUpperCase() : "?"
        font.family: Theme.sansFamily; font.pixelSize: 14; font.weight: Font.DemiBold
        color: root.cls !== "" ? "white" : Theme.ink65
        renderType: Text.NativeRendering
    }
}
