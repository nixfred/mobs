import QtQuick
import MobsCore

// Step 3: pick the microphone, watch it move, lock it in. While this page shows, MOBS listens to the mic (and only listens: nothing is recorded).
Item {
    id: root
    objectName: "setupMic"
    readonly property bool canNext: Prefs.micLocked || !Prefs.micOn
    readonly property bool canSkip: true
    readonly property string skipText: "go without a microphone"
    function skipNow() { Prefs.setMicOn(false) }
    // The first row is "whatever the system uses": it says which microphone that is right now, so the default is not mistaken for a microphone that is not listed
    // (the list changing is what asks again). On 2026-10-13 the system default WAS the Maono, and the page never said so.
    readonly property string defaultName: Audio.microphones.length > 0 ? Audio.defaultMicName() : ""
    readonly property var mics: [{ id: "default", name: defaultName !== "" ? "System default: " + defaultName : "System default microphone" }].concat(Audio.microphones.map(m => ({ id: m.id, name: m.name })))
    Component.onCompleted: { Audio.refresh(); if (!Prefs.micOn) Prefs.setMicOn(true); if (!Engine.active) Engine.checkMic(true) }
    Component.onDestruction: Engine.checkMic(false)

    Row {
        anchors.centerIn: parent
        spacing: 28
        Column {
            width: 340; spacing: 10
            Txt { text: "Pick your microphone"; font.pixelSize: 26; font.bold: true }
            Txt {
                width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 14; color: Omarchy.secondaryText
                text: "Click your microphone and say something. The bar on the right should move. Then press the button."
            }
            Item {   // the list and its scroll bar: a list cut off after a few rows must say there are more
                width: parent.width; height: list.height
                ListView {
                    id: list
                    objectName: "setupMicList"
                    width: parent.width; height: Math.min(Math.max(1, count) * 38 - 4, Math.max(114, root.height - 140)); clip: true
                    model: root.mics; spacing: 4
                    readonly property bool allShown: contentHeight <= height + 1   // every row is in view (else the bar beside the list says there are more)
                    readonly property string firstRow: root.mics.length > 0 ? root.mics[0].name : ""   // for the checks: the "system default" row names the microphone it is
                    delegate: Rectangle {
                        required property var modelData
                        width: list.width - (bar.needed ? 10 : 0); height: 34; radius: 17
                        color: Prefs.micDevice === modelData.id ? Omarchy.accent : "transparent"
                        border.color: Prefs.micDevice === modelData.id ? Omarchy.accent : Omarchy.selection
                        Txt { anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 14 } elide: Text.ElideRight; font.pixelSize: 13; text: modelData.name; color: Prefs.micDevice === modelData.id ? Omarchy.accentForeground : Omarchy.foreground }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: Prefs.setMicDevice(modelData.id) }
                    }
                }
                ListBar { id: bar; view: list }
            }
        }
        Column {
            width: 340; spacing: 12
            LevelMeter { width: parent.width; db: Engine.micDb; receiving: Engine.micFrames; label: "mic" }
            Txt {
                width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 13; color: Omarchy.red
                visible: Engine.audioProblem !== ""
                text: Engine.audioProblem
            }
            BigButton { objectName: "setupLockMic"; text: Prefs.micLocked ? "LOCKED IN" : "THIS IS MY MICROPHONE"; implicitHeight: 46; implicitWidth: 300; enabled: Prefs.micOn && !Prefs.micLocked; onClicked: Prefs.lockMic(Audio.resolveDefault(Prefs.micDevice)) }   // "System default" locks the microphone that is the default now, not whatever becomes it later
            Txt {
                width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 13; color: Omarchy.green
                visible: Prefs.micLocked
                // "default" is left only when the system's default could not be named just now (the audio service did not answer): then it really does follow the system, and says so
                text: Prefs.micDevice === "default" ? "Locked in: your system's default microphone. MOBS could not tell which one that is just now, so it uses whichever the system has."
                                                    : "Locked in: " + Audio.name(Prefs.micDevice) + ". It stays until you change it here."
            }
            Toggle { width: parent.width; label: "also share my computer's sound"; on: Prefs.desktopOn; onToggled: Prefs.setDesktopOn(!Prefs.desktopOn) }
        }
    }
}
