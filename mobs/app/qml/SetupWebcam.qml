import QtQuick
import MobsCore

// Step 5: the webcam on the screen. It is a real window the person can drag, and it follows them across workspaces. The chat sits under it.
Item {
    id: root
    objectName: "setupWebcam"
    readonly property bool canNext: true
    readonly property bool canSkip: false
    Component.onCompleted: App.setSetupPreview(true)

    Row {
        anchors.centerIn: parent
        spacing: 32
        Column {   // a little diagram of the dock: the webcam, and the chat attached under it
            spacing: 6
            Rectangle {
                width: 220; height: 124; radius: 8; color: Omarchy.lighterBackground; border.color: Omarchy.accent; border.width: 2
                Glyph { anchors.centerIn: parent; text: ""; font.pixelSize: 40 }
                Txt { anchors { right: parent.right; top: parent.top; margins: 8 } text: "you"; font.pixelSize: 12; color: Omarchy.secondaryText }
            }
            Rectangle {
                width: 220; height: 92; radius: 8; color: Omarchy.lighterBackground; border.color: Omarchy.selection
                Column { anchors { fill: parent; margins: 10 } spacing: 5
                    Txt { text: "chat"; font.pixelSize: 12; font.bold: true; color: Omarchy.secondaryText }
                    Rectangle { width: 150; height: 8; radius: 4; color: Omarchy.selection }
                    Rectangle { width: 110; height: 8; radius: 4; color: Omarchy.selection }
                    Rectangle { width: 170; height: 8; radius: 4; color: Omarchy.selection }
                }
            }
        }
        Column {
            width: 360; spacing: 10
            Txt { text: "This is your webcam"; font.pixelSize: 26; font.bold: true }
            Txt {
                width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 14; color: Omarchy.secondaryText
                text: App.hasCamera ? "Look at your screen: you are in the top right corner. Drag it anywhere you like. It follows you to every workspace." : "You chose no camera, so there is no webcam window. Go back to the camera page if you want one."
            }
            Txt {
                width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 14; color: Omarchy.foreground
                text: "When people chat on X, their messages appear right under your picture, and they move with it. You answer out loud, so there is nothing to type."
            }
            Txt {
                width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 12; color: Omarchy.tertiaryText
                text: "Bigger or smaller: hold SUPER and drag with the right mouse button."
            }
        }
    }
}
