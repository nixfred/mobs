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
    // The row decides the width (four cards must fit the smallest window); a narrow card uses smaller type.
    width: parent && parent.cardW ? parent.cardW : 252
    readonly property bool narrow: width < 230
    height: 306
    radius: Omarchy.rounding
    color: Omarchy.lighterBackground
    border.width: selected || hover.hovered ? 2 : 1
    border.color: selected || hover.hovered ? Omarchy.accent : Omarchy.selection
    Column {
        anchors.fill: parent
        anchors.margins: root.narrow ? 20 : 26
        spacing: root.narrow ? 10 : 14
        Glyph { text: root.glyph; font.pixelSize: root.narrow ? 42 : 54; horizontalAlignment: Text.AlignLeft }
        Item { width: 1; height: root.narrow ? 2 : 6 }
        Txt { text: root.title; font.pixelSize: root.narrow ? 18 : 22; font.bold: true; width: parent.width; wrapMode: Text.WordWrap }
        Txt { text: root.desc; color: Omarchy.secondaryText; font.pixelSize: root.narrow ? 12 : 13; width: parent.width; wrapMode: Text.WordWrap }
    }
    Key { text: root.keyName; anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 22 }
    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: root.activated() }
}
