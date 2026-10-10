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
        Txt { text: root.caption; font.pixelSize: 11; color: Omarchy.secondaryText }
    }
}
