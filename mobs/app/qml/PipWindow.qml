import QtQuick
import QtMultimedia
import MobsCore

// The webcam as a window ON the screen. MOBS captures the whole screen, so this is what puts you in the picture; the compositor floats and pins it
// (PipController). It is ONE window with up to three parts stacked, so one move carries them all:
//   a guest's card ABOVE you (a viewer you promoted: their video, never their screen), your own camera in the middle, the chat BELOW you.
// The videos keep 16:9 at whatever width the window has (resize the width = resize the videos and the chat's width); the chat card takes the rest of
// the height (resize the height = resize the chat alone). A plain drag anywhere moves the lot (SUPER + drag works too); the edges and the corner resize.
// The grips only show while the pointer is over the window.
Window {
    id: pip
    title: Pip.title            // the compositor is told to float and pin the window with this title (and this process id)
    readonly property bool chatOn: Pip.chatWanted
    readonly property bool guestOn: Pip.guestOn
    readonly property bool docked: chatOn || guestOn
    readonly property int gap: 8
    readonly property real chatMin: 96
    readonly property real cardH: width * 9 / 16
    // Each video card keeps its 16:9 while there is room; a short window squeezes the videos (they crop) before the chat card goes.
    readonly property real availH: height - (chatOn ? gap + chatMin : 0) - (guestOn ? gap : 0)
    readonly property real unit: Math.max(54, Math.min(cardH, availH / (guestOn ? 2 : 1)))
    readonly property real guestH: guestOn ? unit : 0
    readonly property real meY: guestOn ? guestH + gap : 0
    readonly property real meH: !docked ? height : chatOn ? unit : Math.max(54, height - meY)   // without chat, you take the rest of the window
    readonly property real chatY: meY + meH + gap
    width: 480; height: 270 + (guestOn ? gap + 270 : 0) + (chatOn ? gap + 240 : 0)
    minimumWidth: 160; minimumHeight: docked ? (guestOn ? 54 + gap : 0) + 54 + (chatOn ? gap + chatMin : 0) : 90
    color: docked ? "transparent" : "black"     // see-through between the cards, so the desktop shows faintly through the seams
    flags: Qt.Window | Qt.FramelessWindowHint
    transientParent: null       // a window of its own, not a dialog of the main one
    visible: Pip.wanted && win.visible
    onVisibleChanged: { if (visible) Pip.windowShown(); else Pip.windowHidden() }
    // Closed in the middle of a take (a stray SUPER + Q): the compositor's close does not change anything the binding above depends on, so it would stay gone.
    // PipController asks for it back; writing the binding again shows the window if the take still wants it (and the main window is still there).
    Connections {
        target: Pip
        function onReopenRequested() { pip.visible = Qt.binding(function() { return Pip.wanted && win.visible }) }
    }

    CaptureSession {
        camera: Camera {
            cameraDevice: Pip.device; cameraFormat: Pip.format; active: pip.visible && !Pip.testPattern && Pip.device.id !== ""
            onErrorOccurred: (error, errorString) => Pip.cameraProblem(errorString)
            onActiveChanged: Pip.cameraProblem(active ? "camera started" : "camera stopped")
        }
        videoOutput: video
    }
    Rectangle {   // the guest: a viewer you promoted, above you. Their video fills this card once the call is up; until then, who is coming
        id: guestCard
        objectName: "pipGuestCard"
        visible: pip.guestOn
        x: 0; y: 0; width: pip.width; height: pip.guestH
        color: "#0e1522"; clip: true
        readonly property bool pageShown: callLoader.status === Loader.Ready && Call.pageReady   // the call's own picture replaces the placeholder once its page is up
        readonly property real hue: { let h = 7; for (let i = 0; i < Pip.guestName.length; ++i) h = (h * 31 + Pip.guestName.charCodeAt(i)) % 3600; return h / 3600 }
        Column {
            anchors.centerIn: parent; spacing: 5
            visible: !guestCard.pageShown
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.max(28, Math.min(guestCard.height * 0.4, 72)); height: width; radius: width / 2
                color: Qt.hsla(guestCard.hue, 0.5, 0.40, 1)
                Txt { anchors.centerIn: parent; text: Pip.guestName.substring(0, 1).toUpperCase(); font.pixelSize: parent.height * 0.45; font.bold: true; color: "white" }
            }
            Txt { anchors.horizontalCenter: parent.horizontalCenter; text: Pip.guestName; font.pixelSize: Math.max(11, Math.min(18, guestCard.height * 0.1)); font.bold: true }
            Txt {   // a long line (waiting for your guest, paste the invite...) wraps inside the card instead of running out of the window
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.max(40, guestCard.width - 24)
                horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; maximumLineCount: guestCard.height < 100 ? 2 : 3; elide: Text.ElideRight
                text: Pip.guestStatus; font.pixelSize: Math.max(10, Math.min(13, guestCard.height * 0.075)); color: Omarchy.tertiaryText
            }
        }
        Loader {   // the call itself: a Jitsi room in a web view. Loaded only when a call starts, so the web engine costs nothing until a guest comes
            id: callLoader
            objectName: "callLoader"
            anchors { left: parent.left; right: parent.right; top: parent.top; bottom: parent.bottom; bottomMargin: statusBar.visible ? statusBar.height + 12 : 0 }   // the status line sits BELOW the page, never over Jitsi's buttons
            active: Call.active && !Call.standIn && pip.visible
            source: Qt.resolvedUrl("GuestCall.qml")
        }
        Rectangle {   // who: over the call's own page it only shows while the pointer is on the window (Jitsi has its own title there)
            x: 8; y: 8; z: 10
            height: Math.max(20, guestCard.height * 0.1); width: guestWord.implicitWidth + 16; radius: height / 2
            color: Qt.alpha("black", 0.6)
            opacity: guestCard.pageShown && !hover.hovered ? 0 : 1
            Behavior on opacity { NumberAnimation { duration: 160 } }
            Txt { id: guestWord; anchors.centerIn: parent; text: guestCard.pageShown && Pip.guestName !== "guest" ? "GUEST  " + Pip.guestName : "GUEST"; font.pixelSize: Math.max(11, guestCard.height * 0.055); font.bold: true; color: "white" }
        }
        Rectangle {   // what the call is doing, UNDER the page (the placeholder shows this line too, but the page covers it): nobody in the room yet, the sign-in held back, their voice not on the broadcast
            id: statusBar
            objectName: "callStatusBar"
            readonly property bool soundWarn: !Call.problem && Call.state === "connected" && !Prefs.desktopOn   // their voice reaches the broadcast only as desktop sound
            readonly property string line: soundWarn ? "desktop sound is off: viewers cannot hear your guest" : Call.status
            visible: Call.active && guestCard.pageShown && guestCard.height >= 120 && line !== ""
            z: 10
            x: 8; width: guestCard.width - 16
            height: statusText.implicitHeight + 10
            y: guestCard.height - height - 6
            radius: 8
            color: Qt.alpha("black", 0.78)
            border.width: 1; border.color: Call.problem ? Omarchy.red : soundWarn ? Omarchy.yellow : Qt.alpha("white", 0.18)
            Txt {
                id: statusText
                anchors.centerIn: parent; width: parent.width - 14
                text: statusBar.line; wrapMode: Text.Wrap; maximumLineCount: 3; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Math.max(10, Math.min(12, guestCard.height * 0.045)); font.bold: Call.problem || statusBar.soundWarn
                color: Call.problem ? Omarchy.red : statusBar.soundWarn ? Omarchy.yellow : Omarchy.secondaryText
            }
        }
    }
    Rectangle {   // you
        id: videoCard
        objectName: "pipVideoCard"
        x: 0; y: pip.meY; width: pip.width; height: pip.meH
        color: "black"; clip: true
        VideoOutput { id: video; anchors.fill: parent; fillMode: VideoOutput.PreserveAspectCrop; visible: !Pip.testPattern }
        Rectangle {   // a stand-in picture for tests and screenshots: never a real camera
            objectName: "pipPattern"
            anchors.fill: parent; visible: Pip.testPattern; color: "#00c800"
            Txt { anchors.centerIn: parent; text: "webcam"; color: "black"; font.pixelSize: Math.max(12, parent.height / 8); font.bold: true }
        }
        Rectangle {   // what the broadcast is doing, in the corner of the picture: the viewers see it too, which is how streams usually say LIVE
            id: badge
            objectName: "pipBadge"
            z: 10                       // above the picture, whatever is declared after it (the stand-in used to paint over it)
            readonly property bool broadcast: Engine.streamRequested
            readonly property string word: broadcast ? (Engine.streamState === "live" ? "LIVE" : Engine.streamState === "failed" ? "NOT LIVE" : Engine.streamState === "reconnecting" ? "RECONNECTING" : "CONNECTING")
                                                     : (Engine.state === "paused" ? "PAUSED" : "REC")
            visible: broadcast || Engine.state === "recording" || Engine.state === "paused"
            x: 8; y: 8
            height: Math.max(20, videoCard.height * 0.1); width: badgeRow.implicitWidth + 16; radius: height / 2
            color: Qt.alpha("black", 0.6)
            Row {
                id: badgeRow
                anchors.centerIn: parent; spacing: 6
                Rectangle {
                    width: parent.parent.height * 0.4; height: width; radius: width / 2; anchors.verticalCenter: parent.verticalCenter
                    color: badge.word === "LIVE" || badge.word === "REC" ? "#ff4040" : badge.word === "NOT LIVE" ? "transparent" : "#ffc040"
                    border.width: badge.word === "NOT LIVE" ? 2 : 0; border.color: "#ff4040"
                }
                Txt { text: badge.word; font.pixelSize: Math.max(11, videoCard.height * 0.055); font.bold: true; color: "white"; anchors.verticalCenter: parent.verticalCenter }
            }
        }
    }
    ChatPanel {   // the conversation, under you
        id: chatCard
        onMoved: if (dock) dock.follow()
        visible: pip.chatOn
        x: 0; y: pip.chatY
        width: pip.width; height: Math.max(0, pip.height - y)
    }

    // Moving and resizing, by the compositor itself (xdg-toplevel move and resize): no hyprctl calls, so it is as smooth as any window.
    Item {
        id: dock
        anchors.fill: parent
        z: 50
        // Which chat message is the pointer on? The pointer is read HERE, once, for the whole window; the invite chip below floats over that message.
        property bool onMessage: false
        property string inviteName: ""        // the message last pointed at, kept so the chip can still be tapped while it fades
        property real inviteTop: 0
        property real inviteHeight: 0
        function follow() {
            let row = null
            if (hover.hovered && pip.chatOn && !Call.active) row = chatCard.rowAt(hover.point.position.x - chatCard.x, hover.point.position.y - chatCard.y)
            onMessage = row !== null
            if (row) { inviteName = row.name; inviteTop = chatCard.y + row.top; inviteHeight = row.height }
        }
        HoverHandler { id: hover; blocking: false; onPointChanged: dock.follow(); onHoveredChanged: dock.follow() }
        Item {   // a plain drag on your webcam or the chat moves the whole stack. Not over the call page: a drag handler on top of a web page swallows its clicks
                 // (found with a real pointer: Jitsi's buttons did nothing), so that card is left alone. SUPER + drag still moves the window from anywhere.
            anchors { left: parent.left; right: parent.right; top: parent.top; bottom: parent.bottom; margins: 12; topMargin: 12 + (guestCard.pageShown ? guestCard.height : 0) }
            DragHandler { target: null; onActiveChanged: if (active) pip.startSystemMove() }
        }
        component Grip: Item {
            id: grip
            property int edges: 0
            property alias shape: cursor.cursorShape
            HoverHandler { id: cursor }
            DragHandler { target: null; cursorShape: cursor.cursorShape; onActiveChanged: if (active) pip.startSystemResize(grip.edges) }
        }
        Grip {   // the bottom edge changes the height: the chat card alone
            edges: Qt.BottomEdge; shape: Qt.SizeVerCursor
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; rightMargin: 18 }
            height: 12
            Rectangle { anchors.centerIn: parent; width: 44; height: 4; radius: 2; color: Qt.alpha("white", 0.6); opacity: hover.hovered ? 1 : 0; Behavior on opacity { NumberAnimation { duration: 160 } } }
        }
        Grip {   // the right edge changes the width: the videos (and the chat card's width with them)
            edges: Qt.RightEdge; shape: Qt.SizeHorCursor
            anchors { top: parent.top; bottom: parent.bottom; right: parent.right; bottomMargin: 18 }
            width: 12
            Rectangle { anchors.centerIn: parent; width: 4; height: 44; radius: 2; color: Qt.alpha("white", 0.6); opacity: hover.hovered ? 1 : 0; Behavior on opacity { NumberAnimation { duration: 160 } } }
        }
        Grip {   // the corner changes both
            edges: Qt.BottomEdge | Qt.RightEdge; shape: Qt.SizeFDiagCursor
            anchors { right: parent.right; bottom: parent.bottom }
            width: 18; height: 18
            Rectangle { anchors { right: parent.right; bottom: parent.bottom; margins: 3 } width: 9; height: 9; radius: 2; color: "transparent"; border.width: 2; border.color: Qt.alpha("white", 0.6); opacity: hover.hovered ? 1 : 0; Behavior on opacity { NumberAnimation { duration: 160 } } }
        }
        // Calling a guest. Like the grips, these only show while the pointer is over the window.
        component DockChip: Rectangle {
            id: chip
            property string label
            property bool shown: false
            signal tapped()
            height: 24; width: chipText.implicitWidth + 22; radius: 12
            color: Qt.alpha("black", chipTap.pressed ? 0.88 : 0.64)
            border.width: 1; border.color: Qt.alpha("white", 0.28)
            opacity: shown ? 1 : 0
            visible: opacity > 0.01
            Behavior on opacity { NumberAnimation { duration: 160 } }
            Txt { id: chipText; anchors.centerIn: parent; text: chip.label; font.pixelSize: 12; font.bold: true; color: "white" }
            TapHandler { id: chipTap; onTapped: chip.tapped() }
        }
        Row {   // top-right of the guest's card
            visible: Call.active
            x: pip.width - width - 14; y: 8
            spacing: 6
            DockChip { objectName: "copyInviteChip"; label: "copy invite"; shown: hover.hovered; onTapped: Call.copyInvite() }
            DockChip { objectName: "endCallChip"; label: "end call"; shown: hover.hovered; onTapped: Call.end() }
        }
        DockChip {   // over the chat message the pointer is on: bring that viewer into a call, their card joins above your webcam
            objectName: "inviteChip"
            label: "invite " + (dock.inviteName.length > 16 ? dock.inviteName.substring(0, 15) + "..." : dock.inviteName)   // who exactly: the name is on the chip
            visible: opacity > 0.01 && pip.chatOn
            shown: dock.onMessage && !Call.active
            x: chatCard.x + chatCard.width - width - 14; y: dock.inviteTop + (dock.inviteHeight - height) / 2
            onTapped: Call.invite(dock.inviteName)
        }
        DockChip {   // top-right of your own card: bring somebody on without picking them from the chat
            objectName: "addGuestChip"
            visible: opacity > 0.01 && pip.visible
            label: "+ guest"; shown: hover.hovered && !Call.active
            x: pip.width - width - 14; y: pip.meY + 8
            onTapped: Call.invite("")
        }
    }
}
