import QtQuick
import MobsCore

// The big pill for the one thing to press next. On a setup page it is always the loudest thing there.
Rectangle {
    id: root
    property alias text: label.text
    property bool primary: true
    signal clicked()
    implicitWidth: Math.max(200, label.implicitWidth + 64)
    implicitHeight: 56
    radius: 28
    opacity: enabled ? 1 : 0.35
    color: primary ? Omarchy.accent : "transparent"
    border.width: 2
    border.color: primary ? Omarchy.accent : (hover.hovered && enabled ? Omarchy.accent : Omarchy.selection)
    Txt { id: label; anchors.centerIn: parent; font.pixelSize: 18; font.bold: true; color: root.primary ? Omarchy.accentForeground : Omarchy.foreground }
    HoverHandler { id: hover; enabled: root.enabled; cursorShape: Qt.PointingHandCursor }
    TapHandler { enabled: root.enabled; onTapped: root.clicked() }
}
