import QtQuick
import MobsCore

// The last page: what was set up, in plain words, and where the big GO LIVE button is.
Item {
    id: root
    objectName: "setupDone"
    readonly property bool canNext: true
    readonly property bool canSkip: false
    readonly property var items: [
        { ok: App.cameraLocked || App.pick === 1, text: App.pick === 1 ? "No camera: your stream is your screen and your voice" : "Camera: " + (App.hasCamera ? App.cameraName : App.lockedCameraName) },
        { ok: Prefs.micLocked || !Prefs.micOn, text: Prefs.micOn ? "Microphone: " + Audio.name(Prefs.micDevice) : "No microphone" },
        { ok: Preview.hasFrame, text: Preview.hasFrame ? "Screen: your whole screen, every workspace" : "Screen: not shared yet. MOBS asks again when you go live." },
        { ok: Prefs.streamReady, text: Prefs.streamReady ? "X: stream key saved" : "X: no stream key yet. Open the setup guide again to add it." }
    ]
    Column {
        anchors.centerIn: parent
        width: Math.min(700, root.width - 48)
        spacing: 12
        Txt { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: "You are all set"; font.pixelSize: 32; font.bold: true }
        Repeater {
            model: root.items
            Row {
                required property var modelData
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 12
                Txt { text: parent.modelData.ok ? "✓" : "!"; font.pixelSize: 18; font.bold: true; color: parent.modelData.ok ? Omarchy.green : Omarchy.red; width: 22 }
                Txt { text: parent.modelData.text; font.pixelSize: 15; color: parent.modelData.ok ? Omarchy.foreground : Omarchy.red }
            }
        }
        Txt {
            width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; font.pixelSize: 14; color: Omarchy.secondaryText
            text: "Press FINISH. Next time you only press the big red GO LIVE button. You can open this guide again from the ready screen."
        }
    }
}
