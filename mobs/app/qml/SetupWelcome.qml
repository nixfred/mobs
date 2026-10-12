import QtQuick
import MobsCore

// Step 1: what is about to happen and what the person needs.
Item {
    id: root
    objectName: "setupWelcome"
    readonly property bool canNext: true
    readonly property bool canSkip: false
    Column {
        anchors.centerIn: parent
        width: Math.min(780, root.width - 48)
        spacing: 16
        Txt { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: "Let's get you live on X"; font.pixelSize: 32; font.bold: true }
        Txt {
            width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            text: "A few short pages. Each one shows you what to click, with pictures of the real screens. When a page is done, press NEXT."
            font.pixelSize: 15; color: Omarchy.secondaryText
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 14
            Repeater {
                model: [["", "your camera"], ["", "your microphone"], ["", "your whole screen"], ["X", "an X account with Premium"]]
                Rectangle {
                    required property var modelData
                    width: Math.max(130, Math.min(176, (root.width - 48 - 3 * 14) / 4)); height: 92; radius: Omarchy.rounding
                    color: Omarchy.lighterBackground; border.color: Omarchy.selection
                    Column {
                        anchors.centerIn: parent; spacing: 8; width: parent.width - 16
                        Glyph { anchors.horizontalCenter: parent.horizontalCenter; text: parent.parent.modelData[0]; font.pixelSize: 28; font.bold: true }
                        Txt { width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; text: parent.parent.modelData[1]; font.pixelSize: 13 }
                    }
                }
            }
        }
        Txt {
            width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            text: "It takes about ten minutes, and you only do it once. MOBS remembers everything you choose."
            font.pixelSize: 14; color: Omarchy.tertiaryText
        }
    }
}
