import QtQuick
import MobsCore

// How the pieces fit, in three boxes: MOBS sends the picture, through a SOURCE (a named door on X that is made once and has the stream key), into a LIVESTREAM
// (one show, made each time you go live). `highlight` rings the box the page is about: 0 none, 1 the source, 2 the livestream.
Row {
    id: root
    objectName: "flowPicture"
    property int highlight: 0
    spacing: 6
    readonly property real boxW: Math.max(110, (width - 2 * (20 + 2 * spacing)) / 3)
    Repeater {
        model: [{ t: "MOBS", d: "your computer sends the picture" }, { t: "SOURCE", d: "a named door on X, made once. It has the key" }, { t: "LIVESTREAM", d: "one show, made each time you go live" }]
        Row {
            id: cell
            required property var modelData
            required property int index
            readonly property bool lit: root.highlight > 0 && root.highlight === index   // 1 rings the source, 2 the livestream
            spacing: root.spacing
            Rectangle {
                width: root.boxW; height: 68; radius: Omarchy.rounding
                color: Omarchy.lighterBackground
                border.width: cell.lit ? 2 : 1
                border.color: cell.lit ? Omarchy.accent : Omarchy.selection
                Column {
                    anchors.centerIn: parent; spacing: 3; width: parent.width - 14
                    Txt { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: cell.modelData.t; font.pixelSize: 14; font.bold: true; color: cell.lit ? Omarchy.accent : Omarchy.foreground }
                    Txt { width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; text: cell.modelData.d; font.pixelSize: 11; color: Omarchy.secondaryText }
                }
            }
            Txt { visible: cell.index < 2; width: 20; height: 68; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; text: "\u2192"; font.pixelSize: 20; color: Omarchy.tertiaryText }
        }
    }
}
