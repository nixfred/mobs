import QtQuick
import MobsCore

// One of the three big choices on the first screen.
Rectangle {
    id: root
    property string glyph
    property string title
    property string desc
    property string keyName
    property bool selected: false
    signal activated()
    width: 252
    height: 306
    radius: Omarchy.rounding
    color: Omarchy.lighterBackground
    border.width: selected || hover.hovered ? 2 : 1
    border.color: selected || hover.hovered ? Omarchy.accent : Omarchy.selection
    Column {
        anchors.fill: parent
        anchors.margins: 26
        spacing: 14
        Glyph { text: root.glyph; font.pixelSize: 54; horizontalAlignment: Text.AlignLeft }
        Item { width: 1; height: 6 }
        Txt { text: root.title; font.pixelSize: 22; font.bold: true }
        Txt { text: root.desc; color: Omarchy.secondaryText; font.pixelSize: 13; width: parent.width; wrapMode: Text.WordWrap }
    }
    Key { text: root.keyName; anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 22 }
    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: root.activated() }
}
