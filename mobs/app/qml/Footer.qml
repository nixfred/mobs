import QtQuick
import MobsCore

// Bottom strip: the keys that work on this screen, always visible.
Item {
    default property alias hints: row.data
    height: 46
    Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Omarchy.selection; opacity: 0.6 }
    Row {
        id: row
        anchors { left: parent.left; leftMargin: 28; verticalCenter: parent.verticalCenter }
        spacing: 22
    }
    Txt {
        anchors { right: parent.right; rightMargin: 28; verticalCenter: parent.verticalCenter }
        text: "alpha"; color: Omarchy.tertiaryText; font.pixelSize: 11; opacity: 0.8
    }
}
