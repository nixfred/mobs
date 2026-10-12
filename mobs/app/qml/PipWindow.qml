import QtQuick
import QtMultimedia
import MobsCore

// The webcam as a window ON the screen. MOBS captures the whole screen, so this is what puts you in the picture; the compositor floats and pins it
// (PipController). It is ONE window with up to two parts stacked, so one move carries both:
//   your own camera on top, the chat BELOW you.
// The video keeps 16:9 at whatever width the window has (resize the width = resize the video and the chat's width); the chat card takes the rest of
// the height (resize the height = resize the chat alone). A plain drag anywhere moves the lot (SUPER + drag works too); the edges and the corner resize.
// The grips only show while the pointer is over the window.
Window {
    id: pip
    title: Pip.title            // the compositor is told to float and pin the window with this title (and this process id)
    readonly property bool chatOn: Pip.chatWanted
    readonly property int gap: 8
    readonly property real chatMin: 96
    readonly property real cardH: width * 9 / 16
    // The video keeps its 16:9 while there is room; a short window squeezes it (it crops) before the chat card goes.
    readonly property real availH: height - (chatOn ? gap + chatMin : 0)
    readonly property real unit: Math.max(54, Math.min(cardH, availH))
    readonly property real meH: chatOn ? unit : height   // without chat, you take the whole window
    readonly property real chatY: meH + gap
    width: 480; height: 270 + (chatOn ? gap + 240 : 0)
    minimumWidth: 160; minimumHeight: chatOn ? 54 + gap + chatMin : 90
    color: chatOn ? "transparent" : "black"     // see-through between the cards, so the desktop shows faintly through the seams
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
    Rectangle {   // you
        id: videoCard
        objectName: "pipVideoCard"
        x: 0; y: 0; width: pip.width; height: pip.meH
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
