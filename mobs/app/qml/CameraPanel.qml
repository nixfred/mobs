import QtQuick
import MobsCore

// "What are we making, which webcam, and where does it sit." Opens from the camera chip (or by itself when a camera
// recording is asked for without a camera). The diagram uses the SAME rectangle the recording uses.
Item {
    id: root
    visible: App.cameraPanel
    // dim the screen behind and close on a tap outside
    Rectangle { anchors.fill: parent; color: Omarchy.darkerBackground; opacity: 0.72
        TapHandler { onTapped: App.setCameraPanel(false) } }

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: 600
        height: col.implicitHeight + 40
        radius: Omarchy.rounding
        color: Omarchy.darkBackground
        border.color: Omarchy.selection
        border.width: 1
        TapHandler { }   // taps inside the card must not reach the scrim

        Column {
            id: col
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 }
            spacing: root.height < 600 ? 10 : 18

            Row {
                width: parent.width
                Txt { text: "your camera"; font.pixelSize: 20; font.bold: true }
                Item { width: parent.width - 220; height: 1 }
                Row {
                    spacing: 8
                    anchors.verticalCenter: parent.verticalCenter
                    Key { text: "Esc" }
                    Txt { text: "close"; color: Omarchy.tertiaryText; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                }
            }

            // 1. what are we making
            Row {
                spacing: 10
                Repeater {
                    model: [["my screen", 1], ["screen + me", 2], ["just my camera", 3]]
                    Rectangle {
                        required property var modelData
                        width: 172; height: 40; radius: 20
                        color: App.pick === modelData[1] ? Omarchy.accent : "transparent"
                        border.color: App.pick === modelData[1] ? Omarchy.accent : Omarchy.selection
                        border.width: 1
                        Txt { anchors.centerIn: parent; text: parent.modelData[0]; font.pixelSize: 13
                              color: App.pick === parent.modelData[1] ? Omarchy.accentForeground : Omarchy.foreground }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: App.choose(parent.modelData[1]) }
                    }
                }
            }

            // 2. which webcam
            Column {
                width: parent.width
                spacing: 8
                visible: App.pick !== 1
                Txt { text: "which camera"; color: Omarchy.tertiaryText; font.pixelSize: 12 }
                DevicePicker {
                    objectName: "cameraPicker"
                    width: parent.width
                    showDefault: false
                    devices: App.cameras.map(camera => ({id: camera.path, name: camera.name}))
                    selectedId: App.camera
                    onSelected: (id) => App.setCamera(id)
                }
                Txt {
                    visible: App.cameras.length === 0
                    text: "no camera found. plug one in, then tap here to look again."
                    color: Omarchy.red; font.pixelSize: 13
                    TapHandler { onTapped: App.refreshCameras() }
                }
            }

            // 3. where it sits (screen + me only)
            Row {
                spacing: 28
                visible: App.pick === 2
                Column {
                    spacing: 8
                    Txt { text: "tap a corner"; color: Omarchy.tertiaryText; font.pixelSize: 12 }
                    Rectangle {
                        id: diagram
                        width: 280; height: 158; radius: 10
                        color: Omarchy.darkerBackground; border.color: Omarchy.selection; border.width: 1
                        Txt { anchors.centerIn: parent; text: "your screen"; color: Omarchy.tertiaryText; font.pixelSize: 12 }
                        Rectangle {   // the webcam, exactly where the recording puts it
                            // App.corner / App.size are read so the binding re-runs when either changes (a function call alone would not)
            readonly property rect r: { App.corner; App.size; return App.cameraRectNormalized() }
                            x: r.x * diagram.width; y: r.y * diagram.height; width: r.width * diagram.width; height: r.height * diagram.height
                            radius: 4; color: Omarchy.accent; opacity: 0.85
                            Glyph { anchors.centerIn: parent; text: "\uf03d"; font.pixelSize: Math.max(10, parent.height * 0.5); color: Omarchy.accentForeground }
                        }
                        Repeater {
                            model: 4
                            Item {
                                required property int index
                                x: (index % 2) * diagram.width / 2; y: Math.floor(index / 2) * diagram.height / 2
                                width: diagram.width / 2; height: diagram.height / 2
                                HoverHandler { cursorShape: Qt.PointingHandCursor }
                                TapHandler { onTapped: App.setCorner(parent.index) }
                            }
                        }
                    }
                }
                Column {
                    spacing: 8
                    Txt { text: "how big"; color: Omarchy.tertiaryText; font.pixelSize: 12 }
                    Repeater {
                        model: [["small", 0], ["medium", 1], ["large", 2]]
                        Rectangle {
                            required property var modelData
                            width: 140; height: 36; radius: 20
                            color: App.size === modelData[1] ? Omarchy.accent : "transparent"
                            border.color: App.size === modelData[1] ? Omarchy.accent : Omarchy.selection; border.width: 1
                            Txt { anchors.centerIn: parent; text: parent.modelData[0]; font.pixelSize: 13
                                  color: App.size === parent.modelData[1] ? Omarchy.accentForeground : Omarchy.foreground }
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: App.setSize(parent.modelData[1]) }
                        }
                    }
                }
            }

            Txt {
                visible: App.pick === 3
                text: "just my camera fills the whole frame."
                color: Omarchy.tertiaryText; font.pixelSize: 13
            }
            Rectangle {
                width: 140; height: 44; radius: 22; color: Omarchy.accent
                Txt { anchors.centerIn: parent; text: "done"; font.pixelSize: 15; font.bold: true; color: Omarchy.accentForeground }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: App.setCameraPanel(false) }
            }
        }
    }
}
