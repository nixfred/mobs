import QtQuick
import Mobs

// First run, and "new": three big choices.
Item {
    Column {
        anchors.centerIn: parent
        spacing: 36
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 10
            Txt { text: "what are we making?"; font.pixelSize: 34; font.bold: true; anchors.horizontalCenter: parent.horizontalCenter }
            Txt { text: "pick one. you can change it any time."; color: Omarchy.secondaryText; font.pixelSize: 15; anchors.horizontalCenter: parent.horizontalCenter }
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 26
            Card { glyph: ""; title: "my screen"; desc: "everything on your screen, with your mic. the one most people want."; keyName: "1"; selected: App.pick === 1; onActivated: App.choose(1) }
            Card { glyph: ""; title: "screen + me"; desc: "your screen with your camera in the corner. good for walk-throughs."; keyName: "2"; selected: App.pick === 2; onActivated: App.choose(2) }
            Card { glyph: ""; title: "just my camera"; desc: "you and your mic, nothing else. quick messages and takes."; keyName: "3"; selected: App.pick === 3; onActivated: App.choose(3) }
        }
    }
}
