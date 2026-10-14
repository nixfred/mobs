import QtQuick
import MobsCore

// A slim scroll bar for a list that has more rows than its box shows. Without a cue a list cut off after three rows looks complete: on 2026-10-13 the
// Maono microphone was the fifth row of the microphone list, and Fred (rightly) said it was not an option. Put it beside the list, as its sibling.
Rectangle {
    id: root
    required property Flickable view
    readonly property bool needed: view.contentHeight > view.height + 1
    visible: needed
    anchors { right: view.right; top: view.top; bottom: view.bottom }
    width: 5; radius: 2.5; color: Omarchy.selection
    Rectangle {
        width: parent.width; radius: parent.radius; color: Omarchy.accent
        y: root.view.visibleArea.yPosition * parent.height
        height: Math.max(18, root.view.visibleArea.heightRatio * parent.height)
    }
}
