import QtQuick
import Mobs

// One big record button, plain words, and what will be recorded.
Item {
    Column {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -20
        spacing: 22
        Item {
            width: 190; height: 190
            anchors.horizontalCenter: parent.horizontalCenter
            Rectangle { anchors.fill: parent; radius: width / 2; color: "transparent"; border.color: Omarchy.accent; border.width: 4 }
            Rectangle { anchors.centerIn: parent; width: 128; height: 128; radius: 64; color: Omarchy.red }
            HoverHandler { cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: Engine.start(App.pick) }
        }
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 6
            Txt { text: "ready to record"; font.pixelSize: 24; font.bold: true; anchors.horizontalCenter: parent.horizontalCenter }
            Txt { text: App.pickName + (App.pick === 3 ? "" : ", with mic"); color: Omarchy.secondaryText; font.pixelSize: 14; anchors.horizontalCenter: parent.horizontalCenter }
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 8
            Key { text: "Alt" }
            Key { text: "Shift" }
            Key { text: "V" }
            Txt { text: "starts it from anywhere (hotkey not wired yet)"; color: Omarchy.tertiaryText; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter; leftPadding: 8 }
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 12
            Chip { glyph: ""; text: App.displayText; visible: App.pick !== 3 }
            Chip { glyph: ""; text: "mic: default input" }
            Chip { glyph: ""; text: App.pick === 1 ? "camera off" : "camera: not connected yet"; tint: Omarchy.tertiaryText }
        }
        Txt {
            text: Engine.message
            visible: Engine.message !== ""
            width: 640
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: Omarchy.accent
            font.pixelSize: 13
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }
}
