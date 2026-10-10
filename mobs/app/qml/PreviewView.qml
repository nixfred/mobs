import QtQuick
import MobsCore

// What will be recorded, live, at 15 frames per second. Shows the reason instead of a blank box while waiting.
Item {
    id: root
    property real previewWidth: 480
    readonly property real captionHeight: 20
    width: previewWidth
    height: picture.height + captionHeight

    Rectangle {
        id: picture
        width: parent.width; height: Math.round(root.previewWidth * 9 / 16)
        radius: Math.min(Omarchy.rounding, 14)
        color: Omarchy.darkerBackground
        border.color: Omarchy.selection; border.width: 1
        Image {
            anchors { fill: parent; margins: 1 }
            source: Preview.hasFrame ? "image://preview/" + Preview.serial : ""
            cache: false; asynchronous: false; smooth: true
            fillMode: Image.PreserveAspectFit
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
        Rectangle { width: 8; height: 8; radius: 4; color: Omarchy.green; anchors.verticalCenter: parent.verticalCenter }
        Txt { text: "live preview"; font.pixelSize: 11; color: Omarchy.secondaryText }
    }
}
