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
    width: pipDefaultWidth; height: Math.round(pipDefaultWidth * 9 / 16) + (chatOn ? gap + Math.round(pipDefaultWidth / 2) : 0)   // 480 wide when it first opens (MOBS_PIP_WIDTH changes it, for tests)
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
            onErrorOccurred: (error, errorString) => Pip.cameraFailed(errorString)
            onActiveChanged: {
                Pip.cameraProblem(active ? "camera started" : "camera stopped")
                // The window asks for the camera for as long as it is showing: if the camera stops while it still is, nobody asked for it (unplugged, taken by another program)
                if (!active && pip.visible && !Pip.testPattern && Pip.device.id !== "") Pip.cameraFailed("the camera stopped by itself")
            }
        }
        videoOutput: video
    }
    XLiveFinder { x: 0; y: 0 }   // finds the livestream that is on air, so the chat card can open its chat without a copied link
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
            readonly property string word: Chat.ending ? "ENDING" : broadcast ? (Engine.streamState === "live" ? "LIVE" : Engine.streamState === "failed" || Engine.streamState === "ended" ? "NOT LIVE" : Engine.streamState === "reconnecting" ? "RECONNECTING" : "CONNECTING")
                                                     : (Engine.state === "paused" ? "PAUSED" : "REC")
            visible: broadcast || Engine.state === "recording" || Engine.state === "paused" || Chat.ending
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
    // Top right of the picture: how many people are watching and which way that is going, and the KILL SWITCH. The viewers see this too, like the LIVE badge: a viewer count is
    // ordinary on a stream, and the button is a small red pill. One press stops the video this instant and ends the livestream on X (PipController::kill).
    Row {
        id: topRight
        objectName: "pipTopRight"
        z: 60                                          // above the dock's drag zone, so a press lands on the button
        spacing: 8
        visible: Engine.streamRequested && (Engine.active || Chat.ending)
        anchors { top: parent.top; right: parent.right; topMargin: 8; rightMargin: 8 }
        readonly property real h: Math.max(22, videoCard.height * 0.1)
        Rectangle {   // how many are watching
            id: viewersChip
            objectName: "pipViewers"
            visible: Chat.viewers >= 0 && !Chat.ending && pip.width >= 300   // a very narrow window keeps the badge and the kill switch, and drops the count
            height: topRight.h; width: viewersRow.implicitWidth + 20; radius: height / 2
            color: Qt.alpha("black", 0.6)
            readonly property string trendText: Chat.viewerTrend > 0 ? "\u25b2 +" + Chat.viewerDelta : Chat.viewerTrend < 0 ? "\u25bc " + Chat.viewerDelta : Chat.viewerTrendKnown ? "\u25ac" : ""
            readonly property string text: Chat.viewers + " watching " + trendText   // for the checks
            Row {
                id: viewersRow
                anchors.centerIn: parent; spacing: 7
                Txt { text: "\uf06e"; font.pixelSize: Math.round(topRight.h * 0.45); color: "white"; anchors.verticalCenter: parent.verticalCenter }   // an eye
                Txt { objectName: "pipViewersCount"; text: Chat.viewers; font.pixelSize: Math.round(topRight.h * 0.52); font.bold: true; color: "white"; anchors.verticalCenter: parent.verticalCenter }
                Txt {
                    objectName: "pipTrend"
                    visible: viewersChip.trendText !== ""
                    text: viewersChip.trendText
                    font.pixelSize: Math.round(topRight.h * 0.46); font.bold: true; anchors.verticalCenter: parent.verticalCenter
                    color: Chat.viewerTrend > 0 ? "#5be37d" : Chat.viewerTrend < 0 ? "#ff7a7a" : "#c8c8c8"
                }
            }
        }
        Rectangle {   // the kill switch
            id: killButton
            objectName: "pipKill"
            visible: Engine.active
            height: topRight.h; width: killRow.implicitWidth + 24; radius: height / 2
            color: killHover.hovered ? "#ff2a2a" : "#d92020"
            border.width: 2; border.color: Qt.alpha("white", killHover.hovered ? 1 : 0.8)
            readonly property string text: "END"
            Row {
                id: killRow
                anchors.centerIn: parent; spacing: 7
                Rectangle { width: Math.round(topRight.h * 0.3); height: width; radius: 2; color: "white"; anchors.verticalCenter: parent.verticalCenter }
                Txt { text: "END"; font.pixelSize: Math.round(topRight.h * 0.5); font.bold: true; color: "white"; anchors.verticalCenter: parent.verticalCenter }
            }
            HoverHandler { id: killHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: Pip.kill() }
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
