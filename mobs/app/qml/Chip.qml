import QtQuick
import Mobs

// A small pill showing one thing that will be recorded (display, mic, camera).
Rectangle {
    id: root
    property string glyph
    property alias text: label.text
    property color tint: Omarchy.accent
    property bool clickable: false
    signal clicked()
    implicitWidth: row.implicitWidth + 28
    implicitHeight: 38
    radius: 19
    color: Omarchy.lighterBackground
    border.color: clickable && hover.hovered ? Omarchy.accent : Omarchy.selection
    border.width: 1
    Row {
        id: row
        anchors.centerIn: parent
        spacing: 10
        Glyph { text: root.glyph; color: root.tint; font.pixelSize: 16 }
        Txt { id: label; anchors.verticalCenter: parent.verticalCenter; font.pixelSize: 13 }
    }
    HoverHandler { id: hover; enabled: root.clickable; cursorShape: Qt.PointingHandCursor }
    TapHandler { enabled: root.clickable; onTapped: root.clicked() }
}
