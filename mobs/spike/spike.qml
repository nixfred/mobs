import QtQuick

// Hosts the real, unmodified EditBar.qml (copied next to this file) plus a moving
// block so the widget has to keep repainting, and logs evidence through the stub.
Rectangle {
    id: top
    color: "#101014"
    property int frames: 0

    EditBar {
        id: bar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        anchors.verticalCenter: parent.verticalCenter
        height: 64
    }

    Rectangle {
        id: block
        y: 6; width: 24; height: 10; radius: 3; color: "#FFD60A"
        SequentialAnimation on x {
            loops: Animation.Infinite
            NumberAnimation { from: 8; to: Math.max(top.width - 40, 9); duration: 1500 }
            NumberAnimation { from: Math.max(top.width - 40, 9); to: 8; duration: 1500 }
        }
    }

    Text {
        anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 8
        color: "#c8c8cc"; font.pixelSize: 12
        text: "MOBS spike  clips=" + backend.clips.length + "  frames=" + top.frames
    }

    Connections {
        target: top.Window.window
        function onFrameSwapped() { top.frames++ }
    }
    Timer { interval: 2000; running: true; onTriggered: backend.split(30) }
    Timer {
        interval: 6000; running: true
        onTriggered: backend.log("after 6s: frames=" + top.frames + " clips=" + backend.clips.length + " (expect >100 and 2)")
    }
}
