import QtQuick
import MobsCore

Item {
    id: root
    property real db: -60
    property bool receiving: false
    property string label: "mic"
    implicitHeight: 34
    implicitWidth: 280
    Txt {
        text: root.label + ": " + (!root.receiving ? "waiting for audio" : root.db >= -1 ? "too loud" : root.db <= -55 ? "quiet" : Math.round(root.db) + " dB")
        font.pixelSize: 12; color: root.receiving && root.db >= -1 ? Omarchy.red : Omarchy.secondaryText
    }
    Rectangle {
        y: 22; width: parent.width; height: 8; radius: 4; color: Omarchy.selection
        Rectangle {
            width: parent.width * Math.max(0, Math.min(1, (root.db + 60) / 60)); height: parent.height; radius: 4
            color: root.db >= -1 ? Omarchy.red : root.db >= -12 ? Omarchy.accent : Omarchy.green
            Behavior on width { NumberAnimation { duration: 90 } }
        }
    }
}
