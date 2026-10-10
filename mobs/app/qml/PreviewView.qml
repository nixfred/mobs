import QtQuick
import QtQuick.Shapes
import MobsCore

// What will be recorded, live, at 15 frames per second. Shows the reason instead of a blank box while waiting.
Item {
    id: root
    property real previewWidth: 480
    property string caption: "live preview"                 // what the little line under the picture says
    property string overlay: ""                             // a word over a dimmed picture ("paused", "saving..."); empty = none
    property color frameColor: Omarchy.selection            // the picture's outline (red while a take is being saved)
    property color dotColor: Omarchy.green
    property bool draggable: true                           // "screen + me": the webcam in the picture can be dragged, mid-broadcast too
    readonly property real captionHeight: 20
    width: previewWidth
    height: picture.height + captionHeight

    Rectangle {
        id: picture
        width: parent.width; height: Math.round(root.previewWidth * 9 / 16)
        radius: Math.min(Omarchy.rounding, 14)
        color: Omarchy.darkerBackground
        Image {
            anchors { fill: parent; margins: 1 }
            source: Preview.hasFrame ? "image://preview/" + Preview.serial : ""
            cache: false; asynchronous: false; smooth: true
            fillMode: Image.PreserveAspectFit
        }
        Rectangle {   // dims the picture and says why it is not being saved right now
            anchors { fill: parent; margins: 1 }
            radius: parent.radius
            visible: root.overlay !== "" && Preview.hasFrame
            color: Qt.alpha(Omarchy.darkBackground, 0.62)
            Txt { anchors.centerIn: parent; text: root.overlay; font.pixelSize: Math.max(14, Math.min(28, root.previewWidth / 18)); font.bold: true; color: Omarchy.foreground }
        }
        Rectangle {   // a black screen picture is named right on the picture: this is what viewers get
            objectName: "blackPictureBanner"
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 1 }
            height: blackText.implicitHeight + 10
            visible: Engine.pictureProblem !== ""
            color: Qt.alpha(Omarchy.red, 0.92)
            Txt {
                id: blackText
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 8 }
                horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                font.pixelSize: 12; font.bold: true; color: Omarchy.accentForeground
                text: root.previewWidth < 300 ? "BLACK PICTURE" : "BLACK PICTURE: viewers see nothing"   // a tiny picture has no room for the long form
            }
        }
        Repeater {   // the picture is square: paint the four outside-corners in the window colour so it follows the rounded outline
            model: [
                { px: 0, py: 0, path: "M0 0 H R A R R 0 0 0 0 R Z" },
                { px: 1, py: 0, path: "M R 0 H 0 A R R 0 0 1 R R Z" },
                { px: 0, py: 1, path: "M0 R L0 0 A R R 0 0 0 R R Z" },
                { px: 1, py: 1, path: "M R R L R 0 A R R 0 0 1 0 R Z" }
            ]
            Shape {
                id: patch
                required property var modelData
                readonly property real r: Math.max(1, picture.radius)
                x: modelData.px ? picture.width - r : 0; y: modelData.py ? picture.height - r : 0
                width: r; height: r
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeColor: "transparent"; fillColor: Omarchy.background
                    PathSvg { path: patch.modelData.path.replace(/R/g, String(patch.r)) }
                }
            }
        }
        Rectangle {   // the outline, above everything: red while a take is being saved
            anchors.fill: parent; radius: parent.radius; color: "transparent"
            border.color: root.frameColor; border.width: root.frameColor === Omarchy.selection ? 1 : 2
        }
        // Drag the webcam to move it. This is the same picture the audience gets, so what moves here moves there (App.moveCamera applies it
        // live, even in a broadcast): uncover whatever was under it, then put it back, or drop it on another corner.
        Item {
            id: camHandle
            objectName: "webcamHandle"
            visible: root.draggable && App.pick === 2 && !Prefs.pip && Preview.hasFrame   // in pip mode the webcam is a window: SUPER + drag it instead
            readonly property rect r: { App.camera; App.size; App.corner; return App.cameraRectNormalized() }
            x: 1 + r.x * (picture.width - 2); y: 1 + r.y * (picture.height - 2)
            width: r.width * (picture.width - 2); height: r.height * (picture.height - 2)
            Rectangle {
                anchors.fill: parent; color: "transparent"; radius: 2
                border.width: grab.pressed ? 2 : 1
                border.color: grab.pressed || grab.containsMouse ? Omarchy.accent : Qt.alpha(Omarchy.foreground, 0.45)
            }
            MouseArea {
                id: grab
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                property real offX: 0; property real offY: 0
                function nxy(mouse, done) {   // the pointer in the PICTURE's own coordinates: the handle itself moves under it
                    const p = mapToItem(picture, mouse.x, mouse.y)
                    App.moveCamera((p.x - offX - 1) / (picture.width - 2), (p.y - offY - 1) / (picture.height - 2), done)
                }
                onPressed: (mouse) => { const p = mapToItem(picture, mouse.x, mouse.y); offX = p.x - camHandle.x; offY = p.y - camHandle.y }
                onPositionChanged: (mouse) => { if (pressed) nxy(mouse, false) }
                onReleased: (mouse) => nxy(mouse, true)
            }
        }
        Txt {
            anchors.centerIn: parent
            width: parent.width - 40
            visible: !Preview.hasFrame
            horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            color: Omarchy.tertiaryText; font.pixelSize: 13
            text: Engine.message !== "" ? Engine.message : "waiting for the picture..."
        }
    }
    Row {
        objectName: "livePreviewCaption"
        anchors { left: parent.left; leftMargin: 4; top: picture.bottom; topMargin: 6 }
        spacing: 6
        visible: Preview.hasFrame
        Rectangle { width: 8; height: 8; radius: 4; color: root.dotColor; anchors.verticalCenter: parent.verticalCenter }
        Txt { text: root.caption + (camHandle.visible ? "  ·  drag your webcam to move it" : (root.draggable && App.pick === 2 && Prefs.pip ? "  ·  your webcam is a window: SUPER + drag moves it" : "")); font.pixelSize: 11; color: Omarchy.secondaryText }
    }
}
