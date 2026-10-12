import QtQuick
import MobsCore

// Step 4: the whole screen, once. MOBS shares every workspace, so the stream follows the person around. There is no region to choose and
// no window to pick: the small window the system shows is only its way of asking permission, and it is remembered after the first time.
Item {
    id: root
    objectName: "setupScreen"
    readonly property bool canNext: Preview.hasFrame
    readonly property bool canSkip: false
    Component.onCompleted: if (Preview.hasFrame) App.setSetupPreview(true)

    Row {
        anchors.centerIn: parent
        spacing: 28
        PreviewView {
            objectName: "setupScreenPreview"
            previewWidth: 330
            caption: Preview.hasFrame ? "this is what viewers will see" : "waiting for your screen"
            draggable: false
        }
        Column {
            width: 350; spacing: 10
            Txt { text: "Share your whole screen"; font.pixelSize: 26; font.bold: true }
            Txt {
                width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 14; color: Omarchy.secondaryText
                text: "MOBS shows everything on your screen, on every workspace, so the stream follows you wherever you go. Nothing to choose."
            }
            Txt {
                width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 13; color: Omarchy.foreground
                text: "1. Press the button.\n2. Your system opens a small window. Click your whole screen (the big picture) and press Share.\nYou only do this once."
            }
            Row {
                spacing: 10
                BigButton { objectName: "setupShowScreen"; text: Preview.hasFrame ? "SCREEN SHARED" : "SHOW MY SCREEN"; implicitHeight: 46; implicitWidth: 230; enabled: !Preview.hasFrame; onClicked: App.setSetupPreview(true) }
                BigButton { primary: false; text: "choose again"; implicitHeight: 46; implicitWidth: 150; visible: Preview.hasFrame; onClicked: { Engine.forgetScreen(); App.setSetupPreview(true) } }
            }
            Txt {
                width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 13
                visible: Engine.message !== "" || Engine.pictureProblem !== ""
                color: Omarchy.red
                text: Engine.pictureProblem !== "" ? Engine.pictureProblem : Engine.message
            }
        }
    }
}
