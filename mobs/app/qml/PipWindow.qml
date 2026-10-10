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
        readonly property real hue: { let h = 7; for (let i = 0; i < Pip.guestName.length; ++i) h = (h * 31 + Pip.guestName.charCodeAt(i)) % 3600; return h / 3600 }
        Column {
            anchors.centerIn: parent; spacing: 5
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.max(28, Math.min(guestCard.height * 0.4, 72)); height: width; radius: width / 2
                color: Qt.hsla(guestCard.hue, 0.5, 0.40, 1)
                Txt { anchors.centerIn: parent; text: Pip.guestName.substring(0, 1).toUpperCase(); font.pixelSize: parent.height * 0.45; font.bold: true; color: "white" }
            }
            Txt { anchors.horizontalCenter: parent.horizontalCenter; text: Pip.guestName; font.pixelSize: Math.max(11, Math.min(18, guestCard.height * 0.1)); font.bold: true }
            Txt { anchors.horizontalCenter: parent.horizontalCenter; text: Pip.guestStatus; font.pixelSize: Math.max(10, Math.min(13, guestCard.height * 0.075)); color: Omarchy.tertiaryText }
        }
        Rectangle {
            x: 8; y: 8; z: 10
            height: Math.max(20, guestCard.height * 0.1); width: guestWord.implicitWidth + 16; radius: height / 2
            color: Qt.alpha("black", 0.6)
            Txt { id: guestWord; anchors.centerIn: parent; text: "GUEST"; font.pixelSize: Math.max(11, guestCard.height * 0.055); font.bold: true; color: "white" }
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
        visible: pip.chatOn
        x: 0; y: pip.chatY
        width: pip.width; height: Math.max(0, pip.height - y)
    }

    // Moving and resizing, by the compositor itself (xdg-toplevel move and resize): no hyprctl calls, so it is as smooth as any window.
    Item {
        id: dock
        anchors.fill: parent
        z: 50
        HoverHandler { id: hover }
        Item {   // a plain drag anywhere inside the edge zones moves the whole stack
            anchors { fill: parent; margins: 12 }
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
    }
}
