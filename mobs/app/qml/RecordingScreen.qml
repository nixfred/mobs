import QtQuick
import MobsCore

// While recording: big timer, pause and stop. Starting shows what MOBS is waiting for (the screen dialog).
Item {
    readonly property bool live: Engine.state === "recording" || Engine.state === "paused"
    Column {
        anchors.centerIn: parent
        spacing: 26
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 12
            Rectangle {
                width: 16; height: 16; radius: 8; anchors.verticalCenter: parent.verticalCenter
                color: Engine.state === "recording" ? Omarchy.red : Omarchy.tertiaryText
                SequentialAnimation on opacity {
                    running: Engine.state === "recording"; loops: Animation.Infinite
                    NumberAnimation { to: 0.25; duration: 700 } NumberAnimation { to: 1; duration: 700 }
                }
            }
            Txt {
                text: Engine.state === "starting" ? "waiting for you" : Engine.state === "stopping" ? "saving" : Engine.state
                color: Engine.state === "recording" ? Omarchy.red : Omarchy.secondaryText
                font.pixelSize: 20; font.bold: true
            }
        }
        Txt { text: Engine.elapsedText; font.pixelSize: 112; font.bold: true; anchors.horizontalCenter: parent.horizontalCenter }
        Txt {
            text: Engine.message !== "" ? Engine.message : (App.pickName + ", with mic  ·  saving to ~/Videos/mobs")
            color: Omarchy.tertiaryText; font.pixelSize: 13
            width: 700; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 26
            Rectangle {
                width: 88; height: 88; radius: 44; color: "transparent"; border.color: Omarchy.selection; border.width: 3
                opacity: live ? 1 : 0.35
                Glyph { anchors.centerIn: parent; text: Engine.state === "paused" ? "" : ""; font.pixelSize: 30; color: Omarchy.foreground }
                HoverHandler { cursorShape: Qt.PointingHandCursor; enabled: live }
                TapHandler { enabled: live; onTapped: Engine.togglePause() }
            }
            Rectangle {
                width: 88; height: 88; radius: 44; color: Omarchy.accent
                Glyph { anchors.centerIn: parent; text: ""; font.pixelSize: 30; color: Omarchy.accentForeground }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: Engine.stop() }
            }
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 8
            Key { text: "Alt" } Key { text: "Shift" } Key { text: "V" }
            Txt { text: "stop"; color: Omarchy.tertiaryText; font.pixelSize: 13; leftPadding: 8; anchors.verticalCenter: parent.verticalCenter }
        }
    }
}
