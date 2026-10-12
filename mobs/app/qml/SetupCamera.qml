import QtQuick
import QtMultimedia
import MobsCore

// Step 2: pick the camera, see yourself, lock it in. "Locked in" means MOBS uses it every time and never swaps it for another one by itself.
Item {
    id: root
    objectName: "setupCamera"
    readonly property bool canNext: App.cameraLocked && App.hasCamera
    readonly property bool canSkip: true
    readonly property string skipText: App.cameras.length === 0 ? "I have no camera" : "go without a camera"
    function skipNow() { App.choose(1) }   // "my screen": the stream is just the screen and the microphone
    Component.onCompleted: App.refreshCameras()

    Row {
        anchors.centerIn: parent
        spacing: 28
        Rectangle {
            width: 320; height: 180; radius: 10; color: "black"; clip: true
            CaptureSession {
                camera: Camera { cameraDevice: Pip.device; cameraFormat: Pip.format; active: root.visible && !Pip.testPattern && Pip.device.id !== "" }
                videoOutput: video
            }
            VideoOutput { id: video; anchors.fill: parent; fillMode: VideoOutput.PreserveAspectCrop; visible: !Pip.testPattern }
            Rectangle {   // a stand-in picture in tests, never a real camera
                anchors.fill: parent; visible: Pip.testPattern; color: "#00c800"
                Txt { anchors.centerIn: parent; text: "stand-in picture"; color: "black"; font.pixelSize: 14 }
            }
            Txt {
                anchors { fill: parent; margins: 16 }
                visible: !App.hasCamera && !Pip.testPattern
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; wrapMode: Text.WordWrap
                text: App.cameraLocked ? "Your camera, " + App.lockedCameraName + ", is not plugged in. Plug it in, or pick another one." : "No camera found. Plug one in and press look again."
                color: Omarchy.red; font.pixelSize: 14
            }
            Rectangle { anchors.fill: parent; color: "transparent"; radius: 10; border.color: App.cameraLocked ? Omarchy.green : Omarchy.selection; border.width: 2 }
        }
        Column {
            width: 340; spacing: 10
            Txt { text: "Pick your camera"; font.pixelSize: 26; font.bold: true }
            Txt {
                width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 14; color: Omarchy.secondaryText
                text: "That is you on the left. Click your camera, then press the button. MOBS keeps it until you change it."
            }
            ListView {
                id: list
                objectName: "setupCameraList"
                width: parent.width; height: Math.min(Math.max(1, count) * 38, 114); clip: true
                model: App.cameras; spacing: 4
                delegate: Rectangle {
                    required property var modelData
                    width: list.width; height: 34; radius: 17
                    color: App.camera === modelData.path ? Omarchy.accent : "transparent"
                    border.color: App.camera === modelData.path ? Omarchy.accent : Omarchy.selection
                    Txt { anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 14 } elide: Text.ElideRight; font.pixelSize: 13; text: modelData.name; color: App.camera === modelData.path ? Omarchy.accentForeground : Omarchy.foreground }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: App.setCamera(modelData.path) }
                }
            }
            Row {
                spacing: 10
                BigButton { objectName: "setupLockCamera"; text: App.cameraLocked ? "LOCKED IN" : "THIS IS MY CAMERA"; implicitHeight: 46; implicitWidth: 230; enabled: App.hasCamera && !App.cameraLocked; onClicked: App.lockCamera(App.camera) }
                BigButton { primary: false; text: "look again"; implicitHeight: 46; implicitWidth: 130; onClicked: App.refreshCameras() }
            }
            Txt {
                width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 13
                visible: App.cameraLocked
                color: Omarchy.green
                text: "Locked in: " + (App.hasCamera ? App.cameraName : App.lockedCameraName) + ". It stays until you change it here."
            }
        }
    }
}
