import QtQuick
import QtMultimedia

// Spike 2: a video player AND Monologue's real EditBar.qml in one OBS dock, software-rendered.
Rectangle {
    id: top
    color: "#101014"
    function shutdown() { player.stop(); player.source = "" }

    MediaPlayer {
        id: player
        source: backend.videoPath ? "file://" + backend.videoPath : ""
        videoOutput: video.sink
        loops: MediaPlayer.Infinite
        Component.onCompleted: if (backend.videoPath) play()
        onErrorOccurred: (error, errorString) => backend.log("video ERROR " + error + ": " + errorString)
    }

    Image {
        id: vo
        anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 8
        width: 480; height: 270
        cache: false
        asynchronous: false
        fillMode: Image.PreserveAspectFit
        source: "image://frames/" + video.serial
    }

    EditBar {
        id: bar
        anchors.left: parent.left; anchors.right: parent.right
        anchors.margins: 8
        anchors.top: vo.bottom; anchors.topMargin: 24
        height: 64
    }

    Text {
        anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 8
        color: "#c8c8cc"; font.pixelSize: 12
        text: "MOBS spike 2  clips=" + backend.clips.length + "  video state=" + player.playbackState + " pos=" + player.position + "ms"
    }

    Timer { interval: 2000; running: true; onTriggered: backend.split(30) }
    Timer {
        interval: 3000; running: true
        onTriggered: backend.log("t=3s video state=" + player.playbackState + " pos=" + player.position + "ms hasVideo=" + player.hasVideo + " clips=" + backend.clips.length + " | " + video.stats())
    }
    Timer {
        interval: 7000; running: true
        onTriggered: backend.log("t=7s video state=" + player.playbackState + " pos=" + player.position + "ms | " + video.stats())
    }
}
