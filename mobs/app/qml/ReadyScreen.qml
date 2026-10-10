import QtQuick
import MobsCore

// One big record button, plain words, and what will be recorded.
Item {
    Column {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -20
        spacing: 22
        Item {
            width: 190; height: 190
            anchors.horizontalCenter: parent.horizontalCenter
            Rectangle { anchors.fill: parent; radius: width / 2; color: "transparent"; border.color: Omarchy.accent; border.width: 4 }
            Rectangle { anchors.centerIn: parent; width: 128; height: 128; radius: 64; color: Omarchy.red }
            HoverHandler { cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: Engine.start(App.pick) }
        }
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 6
            Txt { text: "ready to record"; font.pixelSize: 24; font.bold: true; anchors.horizontalCenter: parent.horizontalCenter }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 10
                Txt { text: App.pickName + (App.pick === 3 ? "" : ", with mic"); color: Omarchy.secondaryText; font.pixelSize: 14 }
                Rectangle {   // the way back to "what are we making?": my screen / screen + me / just my camera
                    width: changeLabel.implicitWidth + 22; height: 26; radius: 13
                    color: "transparent"
                    border.color: changeHover.hovered ? Omarchy.accent : Omarchy.selection; border.width: 1
                    Txt { id: changeLabel; anchors.centerIn: parent; text: "change"; font.pixelSize: 12; color: Omarchy.accent }
                    HoverHandler { id: changeHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: App.back() }
                }
            }
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 8
            Key { text: "Alt" }
            Key { text: "Shift" }
            Key { text: "V" }
            Txt { text: "starts it from anywhere"; color: Omarchy.tertiaryText; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter; leftPadding: 8 }
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 12
            Chip { glyph: ""; text: App.displayText; visible: App.pick !== 3 }
            Chip { glyph: ""; text: "mic: default input" }
            Chip {
                glyph: "\uf03d"
                clickable: true
                text: App.pick === 1 ? "camera off" : (App.hasCamera ? App.cameraName : "choose a camera")
                tint: App.pick === 1 || !App.hasCamera ? Omarchy.tertiaryText : Omarchy.accent
                onClicked: { App.refreshCameras(); App.setCameraPanel(true) }
            }
        }
        Txt {
            text: Engine.warning
            visible: Engine.warning !== ""
            color: Omarchy.red
            font.pixelSize: 13
            width: 640
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Txt {
            text: Engine.message
            visible: Engine.message !== ""
            width: 640
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: Omarchy.accent
            font.pixelSize: 13
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }
    CameraPanel { anchors.fill: parent; z: 10 }
}
