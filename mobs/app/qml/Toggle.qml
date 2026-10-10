import QtQuick
import MobsCore

// A labelled on/off switch.
Item {
    id: root
    property string label
    property bool on: false
    signal toggled()
    width: parent ? parent.width : 300
    height: 40
    Txt { text: root.label; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 70; elide: Text.ElideRight }
    Rectangle {
        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
        width: 48; height: 26; radius: 13
        color: root.on ? Omarchy.accent : Omarchy.lighterBackground
        border.color: root.on ? Omarchy.accent : Omarchy.selection; border.width: 1
        Rectangle {
            width: 20; height: 20; radius: 10; y: 3
            x: root.on ? parent.width - width - 3 : 3
            color: root.on ? Omarchy.accentForeground : Omarchy.tertiaryText
            Behavior on x { NumberAnimation { duration: 120 } }
        }
    }
    HoverHandler { cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: root.toggled() }
}
