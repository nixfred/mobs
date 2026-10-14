import QtQuick
import MobsCore

// Step 6: your X account, in ONE page. On the right is X's own Live Studio in MOBS's browser. The person signs in there once (the password is typed into X's page: MOBS never sees it); then
// MOBS finds their source, or makes one, and takes the stream key and the server address from X (SourcePilot), so nothing is copied by hand. The same sign-in is what lets the chat of a
// livestream show under the webcam. If MOBS cannot read the key it says why and offers to paste it.
Item {
    id: root
    objectName: "setupX"
    readonly property string phase: pilotItem.phase
    readonly property bool canNext: Prefs.streamReady && Prefs.xSignedIn && (phase === "done" || phase === "failed")
    readonly property bool canSkip: true
    readonly property string skipText: "skip: I will not go live on X"
    property string problem: ""                       // why a key pasted by hand was not taken

    SourcePilot { id: pilotItem; running: root.visible; view: studioLoader.item ? studioLoader.item.view : null }

    Column {
        id: left
        anchors { left: parent.left; leftMargin: 28; top: parent.top }
        width: Math.min(340, Math.max(280, root.width * 0.36))
        spacing: 9
        Txt { width: parent.width; wrapMode: Text.WordWrap; text: "Your X account"; font.pixelSize: 24; font.bold: true }
        Txt {
            visible: pilotItem.phase !== "failed"
            width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 13; color: Omarchy.secondaryText
            text: "Sign in to X in the box, once. MOBS then finds your source and takes its key from X by itself. The same sign-in shows your chat under the webcam."
        }
        Column {
            objectName: "setupXProgress"
            width: parent.width; spacing: 5
            readonly property var flags: [Prefs.xSignedIn, pilotItem.found, pilotItem.keySaved]
            readonly property int firstOpen: { for (let i = 0; i < flags.length; i++) if (!flags[i]) return i; return flags.length }
            Repeater {
                model: ["You are signed in to X", "Your source on X is found" , "Its stream key and address are saved"]
                Row {
                    required property int index
                    required property string modelData
                    width: parent.width; spacing: 8
                    readonly property bool finished: parent.flags[index]
                    readonly property bool current: index === parent.firstOpen
                    Txt { width: 18; text: finished ? "✓" : current ? "●" : "○"; font.pixelSize: 15; color: finished ? Omarchy.green : current ? Omarchy.accent : Omarchy.tertiaryText }
                    Txt {
                        width: parent.width - 26; wrapMode: Text.WordWrap; font.pixelSize: 14; font.bold: current
                        color: finished ? Omarchy.secondaryText : current ? Omarchy.foreground : Omarchy.tertiaryText
                        text: index === 1 && finished && pilotItem.sourceName !== "" ? "Your source on X is found: " + pilotItem.sourceName : modelData
                    }
                }
            }
        }
        Txt {
            objectName: "setupXNote"
            width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 13
            color: pilotItem.phase === "failed" ? Omarchy.red : pilotItem.phase === "signin" ? Omarchy.yellow : pilotItem.phase === "done" ? Omarchy.green : Omarchy.secondaryText
            text: pilotItem.phase === "done" ? "All set. Press NEXT." : pilotItem.note
        }
        Column {   // only when MOBS could not read the key itself
            visible: pilotItem.phase === "failed"
            width: parent.width; spacing: 6
            Txt { width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 12; color: Omarchy.tertiaryText
                text: "In the box open Sources, click the key icon next to your source, copy the Stream Key and press the button." }
            BigButton { objectName: "setupPasteKey"; primary: !Prefs.streamReady; text: Prefs.streamReady ? "PASTE A NEW KEY" : "PASTE THE KEY I COPIED"; implicitHeight: 40; implicitWidth: parent.width; onClicked: root.problem = Prefs.pasteStreamKeyFromClipboard() }
            Txt { visible: root.problem !== "" || Prefs.streamReady; width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 12
                color: root.problem !== "" ? Omarchy.red : Omarchy.green
                text: root.problem !== "" ? root.problem : "Stream key saved. MOBS keeps it hidden in a private file." }
        }
    }
    Rectangle {
        id: box
        objectName: "studioBox"
        anchors { left: left.right; leftMargin: 18; right: parent.right; rightMargin: 28; top: parent.top; bottom: parent.bottom }
        color: "black"; radius: 10; clip: true
        border.width: 2; border.color: pilotItem.phase === "failed" ? Omarchy.red : pilotItem.phase === "done" ? Omarchy.green : Omarchy.selection
        LingerLoader {
            id: studioLoader
            anchors { fill: parent; margins: 2 }
            wanted: root.visible
            sourceComponent: StudioFrame {
                enabled: pilotItem.needsPerson                  // while MOBS works the box is only to look at
                url: pilotItem.running ? xSourcesUrl : "about:blank"
                full: pilotItem.phase === "signin"              // a sign-in form is used at the size of the rest of the interface; a list is shown whole
            }
        }
    }
}
