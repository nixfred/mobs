import QtQuick
import MobsCore

// While recording: big timer, pause and stop. Starting shows what MOBS is waiting for (the screen dialog).
Item {
    id: root
    readonly property bool live: Engine.state === "recording" || Engine.state === "paused"
    Column {
        anchors.centerIn: parent
        spacing: Engine.recordingMic || Engine.recordingDesktop ? 14 : 26
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
        Txt { text: Engine.elapsedText; font.pixelSize: root.height < 600 ? 88 : 112; font.bold: true; anchors.horizontalCenter: parent.horizontalCenter }
        Txt {
            text: Engine.message !== "" ? Engine.message : (App.pickName + (Engine.recordingMic ? ", " + Audio.name(Engine.recordingMicDevice) : ", mic off") + (Engine.recordingDesktop ? ", desktop sound" : "") + "  ·  saving to " + Engine.recordingFolder)
            color: Omarchy.tertiaryText; font.pixelSize: 13
            width: Math.min(700, root.width - 48); horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter; spacing: 24
            LevelMeter { visible: Engine.recordingMic; width: Engine.recordingDesktop ? 280 : 400; db: Engine.micDb; receiving: Engine.micFrames; label: "mic (live input)" }
            LevelMeter { visible: Engine.recordingDesktop; width: Engine.recordingMic ? 280 : 400; db: Engine.desktopDb; receiving: Engine.desktopFrames; label: "desktop (live input)" }
        }
        Txt { text: Engine.audioProblem; visible: text !== ""; width: Math.min(700, root.width - 48); anchors.horizontalCenter: parent.horizontalCenter; wrapMode: Text.WordWrap; font.pixelSize: 13; color: Omarchy.red }
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
