import QtQuick
import MobsCore

// A REAL picture of what the person will see on X (cropped from a real screenshot), with a red ring around what to click.
// The rings are drawn over the picture from fractions of its size, so the picture itself is never altered or invented.
Item {
    id: root
    property string name                   // a file in assets/guide, without the folder
    property var rings: []                 // [{x, y, w, h}] as fractions (0 to 1) of the picture
    property real maxWidth: 640
    property real maxHeight: 200
    readonly property real ratio: picture.sourceSize.width > 0 ? picture.sourceSize.height / picture.sourceSize.width : 0.5
    readonly property real shownWidth: Math.min(maxWidth, maxHeight / Math.max(0.01, ratio))
    implicitWidth: shownWidth
    implicitHeight: shownWidth * ratio
    Image {
        id: picture
        anchors.fill: parent
        source: root.name !== "" ? Qt.resolvedUrl("../assets/guide/" + root.name) : ""
        fillMode: Image.Stretch; smooth: true; asynchronous: false
    }
    Rectangle { anchors.fill: parent; color: "transparent"; radius: 6; border.color: Omarchy.selection; border.width: 1 }
    Repeater {
        model: root.rings
        Rectangle {
            required property var modelData
            objectName: "guideRing"
            x: modelData.x * picture.width - 4; y: modelData.y * picture.height - 4
            width: modelData.w * picture.width + 8; height: modelData.h * picture.height + 8
            radius: 8; color: "transparent"; border.color: Omarchy.red; border.width: 3
        }
    }
}
