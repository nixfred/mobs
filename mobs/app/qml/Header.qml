import QtQuick
import Mobs

// Top strip: the MOBS mark, the current state word, and the expert doors (not wired yet).
Item {
    property string stateWord: ""
    height: 56
    Row {
        anchors { left: parent.left; leftMargin: 28; verticalCenter: parent.verticalCenter }
        spacing: 10
        Glyph { text: ""; font.pixelSize: 14 }
        Txt { text: "mobs"; font.pixelSize: 17; font.bold: true }
        Txt { text: parent.parent.stateWord; color: Omarchy.tertiaryText; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
    }
    Row {
        anchors { right: parent.right; rightMargin: 28; verticalCenter: parent.verticalCenter }
        spacing: 18
        opacity: 0.6   // Pro drawer and full OBS arrive later: shown dimmed, not clickable
        Rectangle {
            width: proLabel.implicitWidth + 24; height: 30; radius: 15
            color: "transparent"; border.color: Omarchy.selection; border.width: 1
            Txt { id: proLabel; anchors.centerIn: parent; text: "pro"; font.pixelSize: 13; color: Omarchy.secondaryText }
        }
        Row {
            spacing: 8
            anchors.verticalCenter: parent.verticalCenter
            Txt { text: "full obs"; color: Omarchy.secondaryText; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
            Glyph { text: ""; font.pixelSize: 13; color: Omarchy.secondaryText }
        }
    }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Omarchy.selection; opacity: 0.6 }
}
