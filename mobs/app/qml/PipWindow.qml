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
        camera: Camera { cameraDevice: Pip.device; cameraFormat: Pip.format; active: pip.visible && !Pip.testPattern && Pip.device.id !== "" }
        videoOutput: video
    }
    VideoOutput { id: video; anchors.fill: parent; fillMode: VideoOutput.PreserveAspectCrop; visible: !Pip.testPattern }
    Rectangle {   // a stand-in picture for tests and screenshots: never a real camera
        objectName: "pipPattern"
        anchors.fill: parent; visible: Pip.testPattern; color: "#00c800"
        Txt { anchors.centerIn: parent; text: "webcam"; color: "black"; font.pixelSize: Math.max(12, parent.height / 8); font.bold: true }
    }
}
