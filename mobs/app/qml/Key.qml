import QtQuick
import MobsCore

// A keycap chip: how shortcuts are shown everywhere in MOBS.
Rectangle {
    property alias text: label.text
    implicitWidth: Math.max(label.implicitWidth + 16, 26)
    implicitHeight: 24
    radius: 6
    color: "transparent"
    border.color: Omarchy.muted
    border.width: 1
    Txt { id: label; anchors.centerIn: parent; font.pixelSize: 12; color: Omarchy.secondaryText }
}
