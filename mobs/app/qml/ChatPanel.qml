import QtQuick
import MobsCore

// The chat of the broadcast as a flow next to you. Read-only: you answer with your voice and your camera, so there is nothing to type into.
// A glass card: a see-through dark panel with a hairline edge. New messages slide in at the bottom, older ones drift up and fade out at the top.
Item {
    id: root
    objectName: "chatPanel"
    readonly property bool compact: width < 300      // a narrow dock drops the avatars and keeps the words
    signal moved()                                   // the messages shifted (one arrived, the list scrolled): whatever the pointer is on may be a different one now
    // The message under a point of this card (panel coordinates), or null: the dock above reads the pointer once for the whole window and uses this to
    // float its "invite to talk" chip over that message. (A hover handler inside a row never received the pointer from under the dock's own.)
    function rowAt(x, y) {
        const p = list.mapFromItem(root, x, y)
        const item = list.itemAt(p.x + list.contentX, p.y + list.contentY)
        return item ? { name: item.name, top: root.mapFromItem(item, 0, 0).y, height: item.height } : null
    }

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: Qt.alpha(Omarchy.darkBackground, 0.82)
        border.width: 1; border.color: Qt.alpha(Omarchy.foreground, 0.12)
    }
    Row {
        id: head
        x: 14; y: 9; height: 16; spacing: 8
        Rectangle {   // alive while the chat is connected
            width: 8; height: 8; radius: 4; anchors.verticalCenter: parent.verticalCenter
            color: Chat.connected ? Omarchy.green : Omarchy.tertiaryText
            SequentialAnimation on opacity { running: Chat.connected; loops: Animation.Infinite; NumberAnimation { to: 0.35; duration: 900 } NumberAnimation { to: 1; duration: 900 } }
        }
        Txt { text: "chat"; font.pixelSize: 12; font.bold: true; color: Omarchy.secondaryText; anchors.verticalCenter: parent.verticalCenter }
        Rectangle {   // made-up messages say so: never let a demo pass for the real thing
            visible: Chat.demo
            anchors.verticalCenter: parent.verticalCenter
            width: demoLabel.implicitWidth + 12; height: 15; radius: 7.5
            color: "transparent"; border.width: 1; border.color: Omarchy.yellow
            Txt { id: demoLabel; anchors.centerIn: parent; text: "DEMO"; font.pixelSize: 10; font.bold: true; color: Omarchy.yellow }
        }
    }
    ListView {
        id: list
        objectName: "chatList"
        visible: Chat.xId === ""
        anchors { left: parent.left; right: parent.right; top: head.bottom; bottom: parent.bottom; leftMargin: 14; rightMargin: 14; topMargin: 6; bottomMargin: 10 }
        clip: true
        spacing: 9
        model: Chat
        interactive: false                      // it follows the conversation by itself
        onContentHeightChanged: { follow(); root.moved() }
        onContentYChanged: root.moved()
        onHeightChanged: follow()
        function follow() { scroll.stop(); scroll.to = Math.max(0, contentHeight - height); scroll.start() }
        NumberAnimation { id: scroll; target: list; property: "contentY"; duration: 240; easing.type: Easing.OutCubic }
        add: Transition { ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200 }
            NumberAnimation { property: "x"; from: 26; to: 0; duration: 260; easing.type: Easing.OutCubic }
        } }
        displaced: Transition { NumberAnimation { property: "y"; duration: 220; easing.type: Easing.OutCubic } }
        delegate: Item {
            id: row
            objectName: "chatRow"
            required property string name
            required property string text
            required property real hue
            required property string initial
            width: list.width
            height: line.implicitHeight
            opacity: Math.max(0, Math.min(1, (y - list.contentY) / 44))      // older messages fade as they near the top edge
            Row {
                id: line
                spacing: 9
                Rectangle {
                    visible: !root.compact
                    width: 28; height: 28; radius: 14
                    color: Qt.hsla(row.hue, 0.5, 0.40, 1)
                    Txt { anchors.centerIn: parent; text: row.initial; font.pixelSize: 13; font.bold: true; color: "white" }
                }
                Column {
                    width: row.width - (root.compact ? 0 : 37)
                    spacing: 1
                    Txt { objectName: "chatName"; width: parent.width; text: row.name; textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: 12; font.bold: true; color: Qt.hsla(row.hue, 0.7, 0.70, 1) }
                    Txt { objectName: "chatBody"; width: parent.width; text: row.text; textFormat: Text.PlainText; wrapMode: Text.Wrap; font.pixelSize: 13 }
                }
            }
        }
    }
    Txt {   // before the first message, and whenever the feed is down: say what is going on instead of showing an empty box
        objectName: "chatEmpty"
        visible: Chat.count === 0 && Chat.xId === ""
        anchors { centerIn: list }
        width: list.width - 8
        horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
        font.pixelSize: 12; color: Omarchy.tertiaryText
        text: Chat.connected ? "waiting for the first message..." : Chat.status
    }
    Loader {   // the chat of the person's own livestream, as X shows it to them: only the "Live Chat" card of its page (XChatView)
        active: Chat.xId !== ""
        anchors { left: parent.left; right: parent.right; top: head.bottom; bottom: parent.bottom; leftMargin: 8; rightMargin: 8; topMargin: 6; bottomMargin: 8 }
        sourceComponent: Component { XChatView {} }
    }
}
