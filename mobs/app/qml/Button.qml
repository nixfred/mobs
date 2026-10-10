import QtQuick
import MobsCore

// A pill button in the Omarchy look. primary = filled with the accent.
Rectangle {
    id: root
    property alias text: label.text
    property bool primary: false
    property color tone: Omarchy.foreground
    signal clicked()
    implicitWidth: label.implicitWidth + 44
    implicitHeight: 44
    radius: 22
    opacity: enabled ? 1 : 0.4
    color: primary ? Omarchy.accent : "transparent"
    border.color: primary ? Omarchy.accent : (hover.hovered && enabled ? Omarchy.accent : Omarchy.selection)
    border.width: 2
    Txt { id: label; anchors.centerIn: parent; font.pixelSize: 14; font.bold: root.primary; color: root.primary ? Omarchy.accentForeground : root.tone }
    HoverHandler { id: hover; enabled: root.enabled; cursorShape: Qt.PointingHandCursor }
    TapHandler { enabled: root.enabled; onTapped: root.clicked() }
}
