import QtQuick
import MobsCore

// Step 8: the stream key. It is the one secret in this whole flow: the person copies it in Live Studio and pastes it here themselves; MOBS keeps it
// in a private file, never shows it again and never puts it in a log.
Item {
    id: root
    objectName: "setupKey"
    readonly property bool canNext: Prefs.streamReady
    readonly property bool canSkip: false
    property string problem: ""
    Column {
        anchors.centerIn: parent
        width: Math.min(780, root.width - 48)
        spacing: 12
        Txt { text: "Copy the stream key"; font.pixelSize: 26; font.bold: true }
        Txt {
            width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 14; color: Omarchy.foreground; lineHeight: 1.25
            text: "In Manage Sources, click the key icon (the red ring) next to YOUR source. X shows a Server URL and a Stream Key. Copy the Stream Key, then press the button below. MOBS already knows the server."
        }
        GuideImage { name: "x-source-card.png"; maxWidth: 560; maxHeight: 80; rings: [{ x: 0.79, y: 0.275, w: 0.045, h: 0.3 }] }
        Row {
            spacing: 12
            BigButton {
                objectName: "setupPasteKey"
                primary: !Prefs.streamReady
                text: Prefs.streamReady ? "PASTE A NEW KEY" : "PASTE THE KEY I COPIED"
                implicitHeight: 46; implicitWidth: 300
                onClicked: root.problem = Prefs.pasteStreamKeyFromClipboard()
            }
            Rectangle {
                width: 260; height: 46; radius: 23; color: Omarchy.lighterBackground; border.color: Omarchy.selection
                TextInput {
                    id: typed
                    objectName: "setupKeyInput"
                    anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                    verticalAlignment: TextInput.AlignVCenter; echoMode: TextInput.Password; clip: true
                    font.family: "JetBrainsMono Nerd Font"; font.pixelSize: 13; color: Omarchy.foreground
                    onAccepted: { root.problem = Prefs.setStreamKey(text); if (root.problem === "") text = "" }
                }
                Txt { anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter } visible: typed.text === ""; text: "or type it here and press Enter"; font.pixelSize: 12; color: Omarchy.tertiaryText }
            }
        }
        Row {   // which source this key belongs to: the go-live window repeats it, so the right one is picked there
            spacing: 10
            Txt { anchors.verticalCenter: parent.verticalCenter; text: "The name of that source:"; font.pixelSize: 13; color: Omarchy.secondaryText }
            Rectangle {
                width: 240; height: 36; radius: 18; color: Omarchy.lighterBackground; border.color: Omarchy.selection
                TextInput {
                    id: sourceInput
                    objectName: "setupSourceName"
                    anchors { fill: parent; leftMargin: 14; rightMargin: 14 }
                    verticalAlignment: TextInput.AlignVCenter; clip: true; maximumLength: 40
                    font.pixelSize: 13; color: Omarchy.foreground
                    Component.onCompleted: text = Prefs.sourceName
                    onTextEdited: Prefs.setSourceName(text)
                }
                Txt { anchors { left: parent.left; leftMargin: 14; verticalCenter: parent.verticalCenter } visible: sourceInput.text === ""; text: "for example MOBS or Gus"; font.pixelSize: 12; color: Omarchy.tertiaryText }
            }
        }
        Txt {
            objectName: "setupKeyStatus"
            width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 14
            color: root.problem !== "" ? Omarchy.red : (Prefs.streamReady ? Omarchy.green : Omarchy.tertiaryText)
            text: root.problem !== "" ? root.problem : (Prefs.streamReady ? "Stream key saved. MOBS keeps it hidden in a private file." : "Waiting for the key.")
        }
    }
}
