import QtQuick
import MobsCore

// While a broadcast is being sent but X does not show it to anyone yet, MOBS's own window stays up and GOES LIVE FOR THE PERSON: on the right is X's Live Studio in MOBS's own browser, with
// MOBS typing the title, waiting until X itself says the video has arrived and pressing Go Live (LivePilot); on the left, in plain words, what is done and what MOBS is waiting for.
// The window steps out of the picture (Pip.hideMainNow) by itself a moment after the livestream exists, because the picture is the whole screen and this page must not be what the
// viewers see. If MOBS cannot do a step it says so in X's own words, and the person finishes by hand in the same box: the old four steps, the "I pressed Go Live myself" button and
// the link to paste stay here for that. With the helper off (MOBS_NO_PILOT=1) there is no box and the window is those steps alone.
Rectangle {
    id: root
    objectName: "goLiveGuide"
    visible: Pip.mainHeld
    color: Omarchy.background
    property bool opened: false
    property string linkNote: ""   // set when the person copied an X link that is not a livestream link
    property string typedNote: ""  // why the text the person TYPED here was not taken: it stays until they type again (the clipboard check below runs every two seconds and would wipe it)
    readonly property bool auto: pilotAllowed                       // MOBS does the steps in the box on the right
    readonly property var pilot: pilotLoader.item                   // the helper (null while the window is not up)
    readonly property string phase: pilot ? pilot.phase : "idle"
    readonly property bool byHand: !auto || phase === "failed"     // the steps are for the person: the helper is off, or it gave up
    readonly property bool wentLive: Chat.xId !== "" || phase === "done"
    function pickUpLink() { linkNote = Chat.attachXFromClipboard() ? "" : Chat.xClipboardNote() }
    Connections {   // a livestream attached by the finder or a copied link: what was said about a typed one is over
        target: Chat
        function onStatusChanged() { if (Chat.xId !== "") root.typedNote = "" }
    }
    TapHandler {}   // clicks stop here: the recording screen underneath is not what this moment is about
    Connections {
        target: Engine
        // Without the helper: once X is receiving the stream, open Live Studio in the person's browser, the next thing to do is there.
        function onStreamChanged() { if (!root.auto && Engine.streamState === "live" && !root.opened) { root.opened = true; App.openUrl("https://studio.x.com/live") } }
        function onStateChanged() { if (Engine.state === "idle") root.opened = false }
    }
    // The livestream exists (the helper made it, or the finder or a pasted link found it): the person is live. A moment to see it say so, then this window leaves the picture.
    Timer { id: leaveSoon; interval: 1600; onTriggered: Pip.hideMainNow() }
    onWentLiveChanged: if (wentLive && auto && Pip.mainHeld) leaveSoon.restart()
    Connections { target: Pip; function onHeldChanged() { if (!Pip.mainHeld) leaveSoon.stop() } }
    // The chat under the webcam: MOBS finds the livestream by itself once it is on air (XLiveFinder). If it cannot, the person presses Copy URL in Live Studio, comes back here, and MOBS picks the link up.
    Connections {
        target: Qt.application
        function onStateChanged() { if (root.visible && Qt.application.state === Qt.ApplicationActive && Chat.xId === "") root.pickUpLink() }
    }
    Timer { interval: 2000; repeat: true; running: root.visible && Prefs.xSignedIn && Chat.xId === "" && root.byHand; onTriggered: root.pickUpLink() }

    // ---- the helper and its box (only while this window is up, and only until the livestream exists: a browser costs memory)
    Loader {
        id: pilotLoader
        active: root.auto && Pip.mainHeld && Chat.xId === ""
        sourceComponent: LivePilot {
            running: Engine.broadcasting
            view: studioLoader.item
            onLive: function (id) { Chat.attachXByHand("https://studio.x.com/live/" + id) }
        }
    }
    Rectangle {
        id: box
        objectName: "studioBox"
        visible: root.auto
        anchors { right: parent.right; rightMargin: 20; top: parent.top; topMargin: 20; bottom: parent.bottom; bottomMargin: 20 }
        width: root.width - left.width - 60
        color: "black"; radius: 10; clip: true
        border.width: 2; border.color: root.phase === "failed" ? Omarchy.red : root.wentLive ? Omarchy.green : Omarchy.selection
        Loader {
            id: studioLoader
            anchors { fill: parent; margins: 2 }
            active: pilotLoader.active
            sourceComponent: XStudioView {
                objectName: "studioView"
                enabled: root.pilot ? root.pilot.needsPerson : false   // while MOBS works the box is only to look at: a stray click must not undo a step
                url: root.pilot && root.pilot.running ? xStudioUrl : "about:blank"
                zoomFactor: Math.max(0.5, Math.min(1, width / 960, height / 560))   // the Create Livestream form is about 930 by 530 pixels: it is shown whole
            }
        }
        Txt { anchors.centerIn: parent; visible: root.wentLive; text: "You're live"; font.pixelSize: 30; font.bold: true; color: Omarchy.green }
    }

    Column {
        id: left
        x: 24; y: 20
        width: root.auto ? Math.min(360, Math.max(300, root.width * 0.34)) : Math.min(880, root.width - 48)
        spacing: root.height < 560 ? 6 : 10
        Txt { width: parent.width; wrapMode: Text.WordWrap; text: root.wentLive ? "You're live on X" : "Going live on X"; font.pixelSize: root.height < 560 ? 20 : 24; font.bold: true }
        Txt {
            objectName: "goLiveStatus"
            width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 13
            color: Engine.streamState === "failed" || Engine.streamState === "ended" ? Omarchy.red : Engine.streamState === "live" ? Omarchy.green : Omarchy.yellow
            text: Engine.streamState === "live" ? "X is receiving your stream" + (Engine.streamKbps > 0 ? " (" + (Engine.streamKbps / 1000).toFixed(1) + " Mbps)" : "")
                : Engine.streamState === "failed" ? "NOT LIVE: " + Engine.streamProblem
                : Engine.streamState === "ended" ? "NOT LIVE: the connection to X was closed."
                : Engine.streamState === "reconnecting" ? Engine.streamProblem : "connecting to X..."
        }
        Column {   // what MOBS does for you, in order
            objectName: "goLiveProgress"
            visible: root.auto && !root.byHand
            width: parent.width; spacing: 6
            readonly property var flags: [Engine.streamState === "live", root.wentLive || (root.pilot ? root.pilot.progress >= 2 : false),
                                          root.wentLive || (root.pilot ? root.pilot.progress >= 3 : false), root.wentLive]
            readonly property int firstOpen: { for (let i = 0; i < flags.length; i++) if (!flags[i]) return i; return flags.length }
            Repeater {
                model: ["Your video reaches X", "MOBS fills in the form: title and who can watch", "X says it sees your video", "MOBS presses Go Live"]
                Row {
                    required property int index
                    required property string modelData
                    width: parent.width; spacing: 8
                    readonly property bool finished: parent.flags[index]
                    readonly property bool current: index === parent.firstOpen
                    Txt { width: 18; text: finished ? "\u2713" : current ? "\u25cf" : "\u25cb"; font.pixelSize: 15; color: finished ? Omarchy.green : current ? Omarchy.accent : Omarchy.tertiaryText }
                    Txt { width: parent.width - 26; wrapMode: Text.WordWrap; text: modelData; font.pixelSize: 14; color: finished ? Omarchy.secondaryText : current ? Omarchy.foreground : Omarchy.tertiaryText; font.bold: current }
                }
            }
        }
        Txt {   // what MOBS is doing right now, and X's own words when it has to wait
            objectName: "goLiveNote"
            visible: root.auto && text !== ""
            width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 13
            color: root.phase === "failed" ? Omarchy.red : root.phase === "signin" || root.phase === "asking" ? Omarchy.yellow : Omarchy.secondaryText
            text: root.wentLive ? "" : root.pilot ? root.pilot.note : ""
        }
        Txt {
            objectName: "goLiveReason"
            visible: root.auto && text !== "" && !root.wentLive
            width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 12; color: Omarchy.tertiaryText
            text: root.pilot && root.pilot.reason !== "" ? "X says: " + root.pilot.reason : ""
        }
        Column {   // the same steps by hand: for when the helper is off or gave up
            visible: root.byHand
            width: parent.width; spacing: 8
            Txt {
                objectName: "goLiveSteps"
                width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: root.height < 560 ? 12 : 13; lineHeight: 1.15
                text: "1. In Live Studio press New Livestream.\n2. Type the title: " + Prefs.liveTitleForX + "\n3. Pick your source" + (Prefs.sourceName !== "" ? ", " + Prefs.sourceName : "") + ". A green dot means MOBS is connected to it.\n4. Choose " + (Prefs.livePublic ? "Public" : "Private") + ", then press Go Live (it stays grey until X has a title and sees your video).\n5. " + (root.auto ? "Then press I'M LIVE here." : "Come back here and press the button below.")
            }
            Txt { visible: !root.auto; width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 12; color: Omarchy.tertiaryText
                text: (Prefs.desktopOn ? "Live Studio's player starts muted: leave it muted, your computer's sound is on air and the player would play it back as an echo."   // (the stream carries what the computer plays)
                                       : "Live Studio's player starts muted: press its speaker icon to hear yourself.") }
        }
        Flow {
            width: parent.width; spacing: 10
            BigButton { objectName: "goLiveOpenStudio"; visible: !root.auto; primary: false; text: "OPEN LIVE STUDIO"; implicitHeight: 44; onClicked: App.openUrl("https://studio.x.com/live") }
            BigButton { objectName: "goLiveHide"; visible: root.byHand; text: root.auto ? "I'M LIVE" : "I'M LIVE: HIDE THIS WINDOW"; implicitWidth: root.auto ? 170 : 350; implicitHeight: 44; onClicked: Pip.hideMainNow() }
            BigButton { objectName: "goLiveStop"; primary: false; text: "STOP"; implicitWidth: 100; implicitHeight: 44; onClicked: Engine.stop() }
        }
        Flow {   // the chat under the webcam: the text, and the place to paste a link (below it when the window is narrow). With the helper at work it is only here when the finder has something to say.
            visible: Prefs.xSignedIn && root.byHand && (!root.auto || Chat.finderNote !== "" || root.linkNote !== "" || root.typedNote !== "")
            width: parent.width; spacing: 8
            Txt {
                objectName: "goLiveChatStatus"
                width: Math.min(left.width, 560); wrapMode: Text.WordWrap; font.pixelSize: 13
                color: root.typedNote !== "" || (Chat.xId === "" && (root.linkNote !== "" || Chat.finderNote !== "")) ? Omarchy.yellow : Chat.xId !== "" ? Omarchy.green : Omarchy.secondaryText
                text: root.typedNote !== "" ? root.typedNote : Chat.xId !== "" ? "Chat attached: it shows under your webcam." : root.linkNote !== "" ? root.linkNote : Chat.finderNote !== "" ? Chat.finderNote : "Chat: MOBS finds your livestream by itself once you are live. If it does not, press Copy URL in Live Studio and paste the link here."
            }
            Rectangle {
                width: 240; height: 36; radius: 18; color: Omarchy.lighterBackground; border.color: Omarchy.selection
                TextInput { id: pasted; objectName: "goLiveLinkInput"; anchors { fill: parent; leftMargin: 14; rightMargin: 14 } verticalAlignment: TextInput.AlignVCenter; clip: true; font.pixelSize: 12; color: Omarchy.foreground
                    onTextEdited: root.typedNote = ""
                    onAccepted: { if (Chat.attachXByHand(text)) { text = ""; root.typedNote = ""; root.linkNote = "" } else root.typedNote = Chat.xTypedNote(text) } }
                Txt { anchors { left: parent.left; leftMargin: 14; verticalCenter: parent.verticalCenter } visible: pasted.text === ""; text: Chat.xId === "" ? "or paste the link here" : "wrong livestream? paste the right link"; font.pixelSize: 12; color: Omarchy.tertiaryText }
            }
        }
        Txt {
            objectName: "goLiveChatLine"
            visible: Prefs.xSignedIn && !root.byHand
            width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 12
            color: Chat.xId !== "" ? Omarchy.green : Omarchy.tertiaryText
            text: Chat.xId !== "" ? "Chat attached: it shows under your webcam." : "The chat will show under your webcam once you are live."
        }
        Txt { visible: root.height >= 520 || !root.byHand; width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 12; color: Omarchy.tertiaryText
            text: root.auto ? "This window leaves the picture by itself once you are live. SUPER + SHIFT + V stops the stream." : "After this window hides, press SUPER + SHIFT + V to stop the stream." }
    }
}
