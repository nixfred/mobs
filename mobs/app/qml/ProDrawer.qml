import QtQuick
import MobsCore

// The expert door: settings that apply to the next recording. Opens from the "pro" chip or the P key.
Rectangle {
    id: drawer
    property int tab: 0
    visible: App.pro
    width: 420
    color: Omarchy.darkBackground
    Rectangle { anchors.left: parent.left; width: 1; height: parent.height; color: Omarchy.selection }
    // swallow taps so they do not reach the screen underneath
    TapHandler { }

    Column {
        anchors { fill: parent; margins: 26 }
        spacing: 18

        Row {
            spacing: 22
            Repeater {
                model: ["recording", "audio", "keys", "about"]
                Column {
                    required property string modelData
                    required property int index
                    spacing: 6
                    Txt { text: modelData; font.pixelSize: 14; font.bold: drawer.tab === index; color: drawer.tab === index ? Omarchy.accent : Omarchy.tertiaryText }
                    Rectangle { width: parent.width; height: 2; color: drawer.tab === index ? Omarchy.accent : "transparent" }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: drawer.tab = parent.index }
                }
            }
        }

        // ---- recording ----
        Column {
            width: parent.width; spacing: 14
            visible: drawer.tab === 0
            Txt { text: "quality"; color: Omarchy.tertiaryText; font.pixelSize: 12 }
            Row {
                spacing: 8
                Repeater {
                    model: [["small", 0], ["balanced", 1], ["high", 2]]
                    Rectangle {
                        required property var modelData
                        width: 112; height: 40; radius: 20
                        color: Prefs.quality === modelData[1] ? Omarchy.accent : "transparent"
                        border.color: Prefs.quality === modelData[1] ? Omarchy.accent : Omarchy.selection; border.width: 1
                        Txt { anchors.centerIn: parent; text: parent.modelData[0]; font.pixelSize: 13; color: Prefs.quality === parent.modelData[1] ? Omarchy.accentForeground : Omarchy.foreground }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: Prefs.setQuality(parent.modelData[1]) }
                    }
                }
            }
            Txt { text: ["smaller files, fine for chat and quick demos", "the sensible default: sharp text, modest size", "the best picture, big files"][Prefs.quality]; color: Omarchy.tertiaryText; font.pixelSize: 12; width: parent.width; wrapMode: Text.WordWrap }

            Txt { text: "save recordings in"; color: Omarchy.tertiaryText; font.pixelSize: 12 }
            Rectangle {
                width: parent.width; height: 42; radius: 14
                color: Omarchy.lighterBackground
                border.color: pathInput.activeFocus ? Omarchy.accent : (pathProblem.text !== "" ? Omarchy.red : Omarchy.selection); border.width: 1
                TextInput {
                    id: pathInput
                    anchors { fill: parent; leftMargin: 14; rightMargin: 14 }
                    verticalAlignment: TextInput.AlignVCenter
                    font.family: "JetBrainsMono Nerd Font"; font.pixelSize: 13
                    color: Omarchy.foreground
                    selectionColor: Omarchy.accent; selectedTextColor: Omarchy.accentForeground
                    clip: true
                    text: Prefs.folder
                    onEditingFinished: { if (text !== Prefs.folder) { pathProblem.text = Prefs.problem(text); if (pathProblem.text === "") Prefs.setFolder(text) } }
                    Connections { target: Prefs; function onChanged() { if (!pathInput.activeFocus) pathInput.text = Prefs.folder } }
                }
            }
            Txt { id: pathProblem; text: ""; color: Omarchy.red; font.pixelSize: 12; visible: text !== "" }
            Row {
                spacing: 10
                Button { text: "open folder"; onClicked: Prefs.openFolder() }
                Button { text: "reset"; enabled: !Prefs.folderIsDefault; onClicked: { Prefs.setFolder(""); pathProblem.text = "" } }
            }

            Toggle { label: "open the editor after recording"; on: Prefs.reviewAfter; onToggled: Prefs.setReviewAfter(!Prefs.reviewAfter) }
            Toggle { label: "live preview (turns the camera on)"; on: App.preview; onToggled: App.setPreview(!App.preview) }
        }

        // ---- audio ----
        Column {
            width: parent.width; spacing: 14
            visible: drawer.tab === 1
            Toggle { label: "record my microphone"; on: Prefs.micOn; onToggled: Prefs.setMicOn(!Prefs.micOn) }
            Txt { text: "uses your default input. With the mic off the recording still has an audio track, silent."; color: Omarchy.tertiaryText; font.pixelSize: 12; width: parent.width; wrapMode: Text.WordWrap }
            Txt { text: "desktop sound, a level meter and per-source volume come later."; color: Omarchy.tertiaryText; font.pixelSize: 12; width: parent.width; wrapMode: Text.WordWrap }
        }

        // ---- keys ----
        Column {
            width: parent.width; spacing: 12
            visible: drawer.tab === 2
            Repeater {
                model: [["Alt", "Shift", "V", "record, or stop and save (from anywhere)"], ["Ctrl", "Alt", "Shift", "V", "pause or resume"], ["Space", "record (here)"], ["P", "this drawer"], ["Esc", "back"]]
                Row {
                    required property var modelData
                    spacing: 6
                    Repeater { model: parent.modelData.slice(0, parent.modelData.length - 1); Key { required property string modelData; text: modelData } }
                    Txt { text: parent.modelData[parent.modelData.length - 1]; color: Omarchy.secondaryText; font.pixelSize: 12; leftPadding: 8; anchors.verticalCenter: parent.verticalCenter }
                }
            }
            Txt { text: "In the editor: S split, X cut, Z zoom, Ctrl+Z undo, Ctrl+S save."; color: Omarchy.tertiaryText; font.pixelSize: 12; width: parent.width; wrapMode: Text.WordWrap }
        }

        // ---- about ----
        Column {
            width: parent.width; spacing: 12
            visible: drawer.tab === 3
            Txt { text: Prefs.aboutText; color: Omarchy.secondaryText; font.pixelSize: 13; width: parent.width; wrapMode: Text.WordWrap; lineHeight: 1.5 }
        }
    }
}
