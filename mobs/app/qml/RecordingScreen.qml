import QtQuick
import MobsCore

// While recording: the live picture of what is being saved, the timer, pause and stop. Starting shows what MOBS is waiting for
// (the screen dialog). The picture is the SAME scene the encoder reads, so what you see is what lands in the file. Wide windows put
// the picture beside the controls, tall ones put it above them; the page never scrolls (Law 17).
Item {
    id: root
    readonly property bool live: Engine.state === "recording" || Engine.state === "paused"
    // During "saving" the last frame stays, dimmed, so the layout does not jump while the file is finished.
    readonly property bool showPreview: Engine.previewing || (Engine.state === "stopping" && Preview.hasFrame)
    readonly property real pad: 24
    readonly property real gap: 28
    readonly property bool wide: root.width >= 700 && root.width >= root.height * 0.85
    readonly property real btn: root.height < 520 ? 72 : 88
    readonly property real previewW: !showPreview ? 0 : wide
        ? Math.max(240, Math.min(840, root.width * 0.58, (root.height - 2 * pad - 28) * 16 / 9))
        : Math.max(200, Math.min(960, root.width - 2 * pad, (root.height - 2 * pad - 28 - controls.height - gap) * 16 / 9))
    readonly property real controlsW: wide && showPreview ? Math.max(260, root.width - 2 * pad - gap - previewW) : Math.min(700, root.width - 2 * pad)

    Item {
        id: stage
        anchors.centerIn: parent
        width: root.wide && root.showPreview ? pv.width + root.gap + controls.width : Math.max(root.showPreview ? pv.width : 0, controls.width)
        height: root.wide && root.showPreview ? Math.max(pv.height, controls.height) : (root.showPreview ? pv.height + root.gap : 0) + controls.height

        PreviewView {
            id: pv
            objectName: "recordingPreview"
            visible: root.showPreview
            previewWidth: root.previewW
            x: root.wide ? 0 : (stage.width - width) / 2
            y: root.wide ? (stage.height - height) / 2 : 0
            frameColor: Engine.state === "recording" ? Omarchy.red : Omarchy.selection
            dotColor: Engine.state === "recording" ? Omarchy.red : Omarchy.tertiaryText
            caption: Engine.broadcasting ? "LIVE: this is what viewers see, and what is being saved" : Engine.state === "recording" ? "recording: this is what is being saved" : Engine.state === "paused" ? "paused: nothing is being saved" : Engine.state === "stopping" ? "finishing the file" : "live preview"
            overlay: Engine.state === "paused" ? "paused" : Engine.state === "stopping" ? "saving..." : ""
        }

        Column {
            id: controls
            width: root.controlsW
            x: root.wide && root.showPreview ? pv.width + root.gap : (stage.width - width) / 2
            y: root.wide && root.showPreview ? (stage.height - height) / 2 : (root.showPreview ? pv.height + root.gap : 0)
            spacing: root.showPreview ? 12 : Engine.recordingMic || Engine.recordingDesktop ? 14 : 26
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
                Rectangle {   // the broadcast, beside the recording: red and solid only while viewers can really see it
                    objectName: "liveBadge"
                    visible: Engine.streamRequested && Engine.state !== "stopping"
                    anchors.verticalCenter: parent.verticalCenter
                    width: badgeText.implicitWidth + 24; height: 28; radius: 14
                    color: Engine.streamState === "live" ? Omarchy.red : "transparent"
                    border.width: 2
                    border.color: Engine.streamState === "failed" ? Omarchy.red : Engine.streamState === "live" ? Omarchy.red : Omarchy.yellow
                    Txt {
                        id: badgeText
                        anchors.centerIn: parent
                        font.pixelSize: 13; font.bold: true
                        text: Engine.streamState === "live" ? "LIVE" : Engine.streamState === "failed" ? "NOT LIVE" : Engine.streamState === "reconnecting" ? "RECONNECTING" : "CONNECTING"
                        color: Engine.streamState === "live" ? Omarchy.accentForeground : Engine.streamState === "failed" ? Omarchy.red : Omarchy.yellow
                    }
                }
            }
            Txt {
                id: liveStatus
                objectName: "liveStatus"
                visible: Engine.streamRequested && Engine.state !== "stopping" && text !== ""
                width: controls.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                font.pixelSize: 13
                color: Engine.streamState === "failed" ? Omarchy.red : Engine.streamState === "live" ? Omarchy.secondaryText : Omarchy.yellow
                text: Engine.streamState === "live" ? "broadcasting at " + (Engine.streamKbps / 1000).toFixed(1) + " Mbps" + (Engine.streamDropped > 0 ? ", " + Engine.streamDropped + " frames dropped" : "")
                    : Engine.streamState === "failed" ? "NOT LIVE: " + Engine.streamProblem + ". The recording is still being saved."
                    : Engine.streamState === "reconnecting" ? Engine.streamProblem
                    : Engine.streamState === "connecting" ? "connecting to the server..." : ""
            }
            Txt {
                text: Engine.elapsedText; font.bold: true; width: controls.width; horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Math.round(Math.min(root.height < 600 ? 88 : 112, controls.width / 5.1))
            }
            Txt {
                text: Engine.message !== "" ? Engine.message : (App.pickName + (Engine.recordingMic ? ", " + Audio.name(Engine.recordingMicDevice) : ", mic off") + (Engine.recordingDesktop ? ", desktop sound" : "") + "  ·  saving to " + Engine.recordingFolder)
                color: Omarchy.tertiaryText; font.pixelSize: 13
                width: controls.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            }
            Item {
                width: controls.width; height: meters.height; visible: Engine.recordingMic || Engine.recordingDesktop
                Grid {
                    id: meters
                    anchors.horizontalCenter: parent.horizontalCenter
                    columns: controls.width >= 560 ? 2 : 1; columnSpacing: 24; rowSpacing: 8
                    LevelMeter {
                        visible: Engine.recordingMic; label: "mic (live input)"; db: Engine.micDb; receiving: Engine.micFrames
                        width: Engine.recordingMic && Engine.recordingDesktop && controls.width >= 560 ? 280 : Math.min(400, controls.width)
                    }
                    LevelMeter {
                        visible: Engine.recordingDesktop; label: "desktop (live input)"; db: Engine.desktopDb; receiving: Engine.desktopFrames
                        width: Engine.recordingMic && Engine.recordingDesktop && controls.width >= 560 ? 280 : Math.min(400, controls.width)
                    }
                }
            }
            Txt { text: Engine.audioProblem; visible: text !== ""; width: controls.width; wrapMode: Text.WordWrap; font.pixelSize: 13; color: Omarchy.red }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 26
                Rectangle {
                    width: root.btn; height: root.btn; radius: root.btn / 2; color: "transparent"; border.color: Omarchy.selection; border.width: 3
                    opacity: live && !Engine.broadcasting ? 1 : 0.35
                    Glyph { anchors.centerIn: parent; text: Engine.state === "paused" ? "" : ""; font.pixelSize: 30; color: Omarchy.foreground }
                    HoverHandler { cursorShape: Qt.PointingHandCursor; enabled: live && !Engine.broadcasting }
                    TapHandler { enabled: live && !Engine.broadcasting; onTapped: Engine.togglePause() }
                }
                Rectangle {
                    width: root.btn; height: root.btn; radius: root.btn / 2; color: Omarchy.accent
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
}
