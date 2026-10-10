import QtQuick
import MobsCore

// Top strip: the MOBS mark, the current state word, and the Pro door (opens the drawer).
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
        Rectangle {
            width: proLabel.implicitWidth + 24; height: 30; radius: 15
            color: App.pro ? Omarchy.accent : "transparent"
            border.color: App.pro || proHover.hovered ? Omarchy.accent : Omarchy.selection; border.width: 1
            Txt { id: proLabel; anchors.centerIn: parent; text: App.pro ? "close EXPERT" : "EXPERT"; font.pixelSize: 13; font.bold: App.pro; color: App.pro ? Omarchy.accentForeground : Omarchy.secondaryText }
            HoverHandler { id: proHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: App.setPro(!App.pro) }
        }
    }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Omarchy.selection; opacity: 0.6 }
}
