import QtQuick
import Mobs

// The MOBS window. Screens so far: choose and ready. Everything follows the live Omarchy theme.
Window {
    id: win
    visible: true
    width: 1180
    height: 720
    minimumWidth: 860
    minimumHeight: 560
    title: "mobs"
    color: Omarchy.background

    Header {
        id: header
        anchors { top: parent.top; left: parent.left; right: parent.right }
        stateWord: App.screen === 1 ? "new" : App.screen === 3 ? Engine.state : "ready"
    }
    ChooseScreen {
        visible: App.screen === 1
        anchors { top: header.bottom; bottom: footer.top; left: parent.left; right: parent.right }
    }
    ReadyScreen {
        visible: App.screen === 2
        anchors { top: header.bottom; bottom: footer.top; left: parent.left; right: parent.right }
    }
    RecordingScreen {
        visible: App.screen === 3
        anchors { top: header.bottom; bottom: footer.top; left: parent.left; right: parent.right }
    }
    Footer {
        id: footer
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        Row { spacing: 8; visible: App.screen === 1; Key { text: "1" } Key { text: "2" } Key { text: "3" } Txt { text: "pick"; color: Omarchy.tertiaryText; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
        Row { spacing: 8; visible: App.screen === 2; Key { text: "Space" } Txt { text: "record"; color: Omarchy.tertiaryText; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
        Row { spacing: 8; visible: App.screen === 3; Key { text: "Space" } Txt { text: "pause / resume"; color: Omarchy.tertiaryText; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
        Row { spacing: 8; visible: App.screen === 3; Key { text: "Enter" } Txt { text: "stop and save"; color: Omarchy.tertiaryText; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
        Row { spacing: 8; visible: App.screen === 2; Key { text: "Esc" } Txt { text: "change what we're making"; color: Omarchy.tertiaryText; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
    }

    Item {
        focus: true
        Keys.onPressed: (e) => {
            if (App.screen === 1 && e.key >= Qt.Key_1 && e.key <= Qt.Key_3) App.choose(e.key - Qt.Key_0)
            else if (App.screen === 2 && e.key === Qt.Key_Space) Engine.start(App.pick)
            else if (App.screen === 3 && e.key === Qt.Key_Space) Engine.togglePause()
            else if (App.screen === 3 && (e.key === Qt.Key_Return || e.key === Qt.Key_Enter)) Engine.stop()
            else if (App.screen === 2 && e.key === Qt.Key_Escape) { if (App.cameraPanel) App.setCameraPanel(false); else App.back() }
        }
    }
}
