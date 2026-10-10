import QtQuick
import QtMultimedia
import MobsCore

// The webcam as a window ON the screen. MOBS captures the whole screen, so this is what puts you in the picture; the compositor floats and pins it
// (PipController) and you move it with SUPER + left-drag, even in the middle of a broadcast. Resize with SUPER + right-drag.
Window {
    id: pip
    title: Pip.title            // the compositor is told to float and pin the window with this title (and this process id)
    width: 480; height: 270
    minimumWidth: 160; minimumHeight: 90
    color: "black"
    flags: Qt.Window | Qt.FramelessWindowHint
    transientParent: null       // a window of its own, not a dialog of the main one
    visible: Pip.wanted && win.visible
    onVisibleChanged: { if (visible) Pip.windowShown(); else Pip.windowHidden() }

    CaptureSession {
        camera: Camera {
            cameraDevice: Pip.device; cameraFormat: Pip.format; active: pip.visible && !Pip.testPattern && Pip.device.id !== ""
            onErrorOccurred: (error, errorString) => Pip.cameraProblem(errorString)
            onActiveChanged: Pip.cameraProblem(active ? "camera started" : "camera stopped")
        }
        videoOutput: video
    }
    VideoOutput { id: video; anchors.fill: parent; fillMode: VideoOutput.PreserveAspectCrop; visible: !Pip.testPattern }
    Rectangle {   // what the broadcast is doing, in the corner of the picture: the viewers see it too, which is how streams usually say LIVE
        id: badge
        objectName: "pipBadge"
        z: 10                       // above the picture, whatever is declared after it (the stand-in used to paint over it)
        readonly property bool broadcast: Engine.streamRequested
        readonly property string word: broadcast ? (Engine.streamState === "live" ? "LIVE" : Engine.streamState === "failed" ? "NOT LIVE" : Engine.streamState === "reconnecting" ? "RECONNECTING" : "CONNECTING")
                                                 : (Engine.state === "paused" ? "PAUSED" : "REC")
        visible: broadcast || Engine.state === "recording" || Engine.state === "paused"
        x: 8; y: 8
        height: Math.max(20, pip.height * 0.1); width: badgeRow.implicitWidth + 16; radius: height / 2
        color: Qt.alpha("black", 0.6)
        Row {
            id: badgeRow
            anchors.centerIn: parent; spacing: 6
            Rectangle {
                width: parent.parent.height * 0.4; height: width; radius: width / 2; anchors.verticalCenter: parent.verticalCenter
                color: badge.word === "LIVE" || badge.word === "REC" ? "#ff4040" : badge.word === "NOT LIVE" ? "transparent" : "#ffc040"
                border.width: badge.word === "NOT LIVE" ? 2 : 0; border.color: "#ff4040"
            }
            Txt { text: badge.word; font.pixelSize: Math.max(11, pip.height * 0.055); font.bold: true; color: "white"; anchors.verticalCenter: parent.verticalCenter }
        }
    }
    Rectangle {   // a stand-in picture for tests and screenshots: never a real camera
        objectName: "pipPattern"
        anchors.fill: parent; visible: Pip.testPattern; color: "#00c800"
        Txt { anchors.centerIn: parent; text: "webcam"; color: "black"; font.pixelSize: Math.max(12, parent.height / 8); font.bold: true }
    }
}
