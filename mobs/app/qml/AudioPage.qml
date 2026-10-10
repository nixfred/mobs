import QtQuick
import MobsCore

Item {
    id: root
    property bool details: false
    Row {
        width: parent.width; spacing: 28
        Column {
            width: (parent.width - parent.spacing) / 2; spacing: 6
            visible: !root.details
            Toggle { label: "record microphone"; on: Prefs.micOn; onToggled: Prefs.setMicOn(!Prefs.micOn) }
            DevicePicker { width: parent.width; devices: Audio.microphones; selectedId: Prefs.micDevice; defaultLabel: "system microphone"; onSelected: (id) => Prefs.setMicDevice(id) }
            Row {
                spacing: 8
                Button { height: 40; text: Engine.micChecking ? "stop check" : "check mic"; enabled: !Engine.active && Prefs.micOn; onClicked: Engine.checkMic(!Engine.micChecking) }
                Button { text: "EXPERT gain"; height: 40; onClicked: root.details = true }
            }
            LevelMeter { width: parent.width; db: Engine.micDb; receiving: Engine.micFrames; label: "mic" }
        }
        Column {
            width: (parent.width - parent.spacing) / 2; spacing: 6
            visible: !root.details
            Toggle { label: "record desktop sound"; on: Prefs.desktopOn; onToggled: Prefs.setDesktopOn(!Prefs.desktopOn) }
            DevicePicker { width: parent.width; devices: Audio.desktops; selectedId: Prefs.desktopDevice; defaultLabel: "system output"; onSelected: (id) => Prefs.setDesktopDevice(id) }
            Row {
                spacing: 8
                Button { height: 40; text: Audio.scanning ? "looking…" : "refresh"; enabled: !Audio.scanning; onClicked: Audio.refresh() }
                Button { text: "EXPERT gain"; height: 40; onClicked: root.details = true }
            }
            LevelMeter { width: parent.width; db: Engine.desktopDb; receiving: Engine.desktopFrames; label: "desktop" }
        }
        Column {
            visible: root.details
            width: (parent.width - parent.spacing) / 2; spacing: 12
            Txt { text: "microphone gain"; font.pixelSize: 16; font.bold: true }
            Row {
                spacing: 10
                Button { text: "−"; enabled: Prefs.micGainDb > -60; onClicked: Prefs.setMicGainDb(Math.max(-60, Prefs.micGainDb - 3)) }
                Txt { text: Prefs.micGainDb + " dB"; anchors.verticalCenter: parent.verticalCenter; font.pixelSize: 16 }
                Button { text: "+"; enabled: Prefs.micGainDb < 12; onClicked: Prefs.setMicGainDb(Math.min(12, Prefs.micGainDb + 3)) }
                Button { text: "reset"; onClicked: Prefs.setMicGainDb(0) }
            }
            LevelMeter { width: parent.width; db: Engine.micDb; receiving: Engine.micFrames }
            Txt { width: parent.width; wrapMode: Text.WordWrap; text: "0 dB leaves the input unchanged. Aim below −12 dB while talking. Too loud means clipping. Checking captures the mic without saving or playing it."; font.pixelSize: 12; color: Omarchy.secondaryText }
            Button { text: "back to inputs"; onClicked: root.details = false }
        }
        Column {
            visible: root.details
            width: (parent.width - parent.spacing) / 2; spacing: 12
            Txt { text: "desktop gain"; font.pixelSize: 16; font.bold: true }
            Row {
                spacing: 10
                Button { text: "−"; enabled: Prefs.desktopGainDb > -60; onClicked: Prefs.setDesktopGainDb(Math.max(-60, Prefs.desktopGainDb - 3)) }
                Txt { text: Prefs.desktopGainDb + " dB"; anchors.verticalCenter: parent.verticalCenter; font.pixelSize: 16 }
                Button { text: "+"; enabled: Prefs.desktopGainDb < 12; onClicked: Prefs.setDesktopGainDb(Math.min(12, Prefs.desktopGainDb + 3)) }
                Button { text: "reset"; onClicked: Prefs.setDesktopGainDb(-6) }
            }
            LevelMeter { width: parent.width; db: Engine.desktopDb; receiving: Engine.desktopFrames; label: "desktop" }
            Txt { width: parent.width; wrapMode: Text.WordWrap; text: "Captures the selected output's monitor, including app and notification sounds. The mic and desktop mix into one AAC track. Desktop levels appear during recording."; font.pixelSize: 12; color: Omarchy.secondaryText }
        }
    }
    Txt {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        text: Engine.audioProblem !== "" ? Engine.audioProblem : Audio.error
        visible: text !== ""; wrapMode: Text.WordWrap; font.pixelSize: 12; color: Omarchy.red
    }
}
