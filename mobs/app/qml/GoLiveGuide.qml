import QtQuick
import MobsCore

// While a broadcast is being sent but X does not show it to anyone yet, MOBS's own window stays up with what to do next: the real Live Studio
// dialog, the four steps, and one button to press when the person has pressed Go Live. Only then does the window step out of the picture
// (Pip.hideMainNow), because the picture is the whole screen and this page must not be what the viewers see.
Rectangle {
    id: root
    objectName: "goLiveGuide"
    visible: Pip.mainHeld
    color: Omarchy.background
    property bool opened: false
    property string linkNote: ""   // set when the person copied an X link that is not a livestream link
    function pickUpLink() { linkNote = Chat.attachXFromClipboard() ? "" : Chat.xClipboardNote() }
    TapHandler {}   // clicks stop here: the recording screen underneath is not what this moment is about
    Connections {
        target: Engine
        // Once X is receiving the stream, open Live Studio for the person: the next thing to do is there.
        function onStreamChanged() { if (Engine.streamState === "live" && !root.opened) { root.opened = true; App.openUrl("https://studio.x.com/live") } }
        function onStateChanged() { if (Engine.state === "idle") root.opened = false }
    }
    // The chat under the webcam: MOBS finds the livestream by itself once it is on air (XLiveFinder). If it cannot, the person presses Copy URL in Live Studio, comes back here, and MOBS picks the link up.
    Connections {
        target: Qt.application
        function onStateChanged() { if (root.visible && Qt.application.state === Qt.ApplicationActive && Chat.xId === "") root.pickUpLink() }
    }
    Timer { interval: 2000; repeat: true; running: root.visible && Prefs.xSignedIn && Chat.xId === ""; onTriggered: root.pickUpLink() }
    Column {
        anchors.centerIn: parent
        width: Math.min(880, root.width - 48)
        spacing: root.height < 560 ? 7 : 12   // the smallest window has no room to spare
        Txt { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: "Your video is on its way to X. Nobody can see it yet."; font.pixelSize: root.height < 560 ? 21 : 24; font.bold: true }
        Txt {
            objectName: "goLiveStatus"
            width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; font.pixelSize: 13
            color: Engine.streamState === "failed" ? Omarchy.red : Engine.streamState === "live" ? Omarchy.green : Omarchy.yellow
            text: Engine.streamState === "live" ? "X is receiving your stream" + (Engine.streamKbps > 0 ? " (" + (Engine.streamKbps / 1000).toFixed(1) + " Mbps)" : "")
                : Engine.streamState === "failed" ? "NOT LIVE: " + Engine.streamProblem
                : Engine.streamState === "reconnecting" ? Engine.streamProblem : "connecting to X..."
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 22
            GuideImage {
                name: "x-create-livestream.png"; maxWidth: 360; maxHeight: root.height < 560 ? 150 : 190
                rings: [{ x: 0.26, y: 0.399, w: 0.575, h: 0.073 }, { x: 0.854, y: 0.901, w: 0.086, h: 0.06 }]
            }
            Column {
                width: Math.min(400, root.width - 48 - 400 - 22); spacing: 8
                Txt {
                    objectName: "goLiveSteps"
                    width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 14; lineHeight: 1.2
                    text: "1. In Live Studio press New Livestream.\n2. Pick your source" + (Prefs.sourceName !== "" ? ", " + Prefs.sourceName : "") + ". A green dot means MOBS is connected to it.\n3. Choose Public (or Private to try it out), then press Go Live.\n4. Press Post livestream on X so your followers know."
                }
                Txt { width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 12; color: Omarchy.tertiaryText; text: "MOBS opened Live Studio. No green dot? You picked another source than the one whose key MOBS has. Live Studio's player starts muted: press its speaker icon to hear yourself." }
            }
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 14
            BigButton { objectName: "goLiveOpenStudio"; primary: false; text: "OPEN LIVE STUDIO"; implicitHeight: 50; onClicked: App.openUrl("https://studio.x.com/live") }
            BigButton { objectName: "goLiveHide"; text: "I'M LIVE: HIDE THIS WINDOW"; implicitHeight: 50; onClicked: Pip.hideMainNow() }
            BigButton { objectName: "goLiveStop"; primary: false; text: "STOP"; implicitWidth: 120; implicitHeight: 50; onClicked: Engine.stop() }
        }
        Row {   // the chat under the webcam
            visible: Prefs.xSignedIn
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 12
            Txt {
                objectName: "goLiveChatStatus"
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(560, root.width - 48 - 260); wrapMode: Text.WordWrap; font.pixelSize: 13
                color: Chat.xId !== "" ? Omarchy.green : root.linkNote !== "" ? Omarchy.yellow : Omarchy.secondaryText
                text: Chat.xId !== "" ? "Chat attached: it shows under your webcam." : root.linkNote !== "" ? root.linkNote : "Chat: MOBS finds your livestream by itself once you have pressed Go Live. If it does not, press Copy URL in Live Studio and come back."
            }
            Rectangle {
                visible: Chat.xId === ""
                width: 240; height: 36; radius: 18; color: Omarchy.lighterBackground; border.color: Omarchy.selection
                TextInput { id: pasted; objectName: "goLiveLinkInput"; anchors { fill: parent; leftMargin: 14; rightMargin: 14 } verticalAlignment: TextInput.AlignVCenter; clip: true; font.pixelSize: 12; color: Omarchy.foreground; onAccepted: { if (Chat.attachX(text)) text = "" } }
                Txt { anchors { left: parent.left; leftMargin: 14; verticalCenter: parent.verticalCenter } visible: pasted.text === ""; text: "or paste the link here"; font.pixelSize: 12; color: Omarchy.tertiaryText }
            }
        }
        Txt { width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; font.pixelSize: 12; color: Omarchy.tertiaryText; text: "After this window hides, press SUPER + SHIFT + V to stop the stream." }
    }
}
