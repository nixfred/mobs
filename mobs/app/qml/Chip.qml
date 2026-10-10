import QtQuick
import Mobs

// A small pill showing one thing that will be recorded (display, mic, camera).
Rectangle {
    id: root
    property string glyph
    property alias text: label.text
    property color tint: Omarchy.accent
    implicitWidth: row.implicitWidth + 28
    implicitHeight: 38
    radius: 19
    color: Omarchy.lighterBackground
    border.color: Omarchy.selection
    border.width: 1
    Row {
        id: row
        anchors.centerIn: parent
        spacing: 10
        Glyph { text: root.glyph; color: root.tint; font.pixelSize: 16 }
        Txt { id: label; anchors.verticalCenter: parent.verticalCenter; font.pixelSize: 13 }
    }
}
