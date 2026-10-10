import QtQuick
import QtMultimedia
import MobsCore

// After a recording: watch it, trim and cut it on Monologue's timeline, save the edit. The original is never changed.
Item {
    id: root
    property bool confirmLeave: false
    property bool confirmDelete: false
    readonly property real videoWidth: Math.max(320, Math.min(720, (height - 330) * 16 / 9, width - 80))

    function fmt(t) {
        t = Math.max(0, Math.round(t))
        var h = Math.floor(t / 3600), m = Math.floor(t / 60) % 60, s = t % 60
        return (h > 0 ? h + ":" : "") + (m < 10 && h > 0 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s
    }

    // ---- playback: plays only the kept clips and hops over the gaps (as Monologue does) ----
    function playableFrom(t) {
        var clips = Review.clips
        for (var i = 0; i < clips.length; ++i)
            if (t < clips[i].end - 0.03) return Math.max(t, clips[i].start)
        return -1
    }
    function seekTo(t) {
        t = Math.max(0, Math.min(t, Review.duration))
        bar.playheadSec = t
        player.position = Math.round(t * 1000)
    }
    function togglePlayback() {
        if (player.playbackState === MediaPlayer.PlayingState) { player.pause(); return }
        var from = playableFrom(bar.playheadSec)
        seekTo(from < 0 ? Review.clips[0].start : from)
        player.play()
    }
    function gapAt(t) { var n = 0; while (n < Review.clips.length && Review.clips[n].end <= t) ++n; return n }
    function restoreGap(gap) {
        var clips = Review.clips
        if (gap === 0) Review.setClip(0, 0, clips[0].end)
        else if (gap === clips.length) Review.setClip(gap - 1, clips[gap - 1].start, Review.duration)
        else Review.joinClips(gap - 1)
    }
    function cutOrRestore() {
        var i = bar.clipAt(bar.playheadSec)
        if (i >= 0) Review.removeClip(i)
        else restoreGap(gapAt(bar.playheadSec))
    }
    function jumpToMark(direction) {
        var marks = Review.pauses.slice()
        for (var c = 0; c < Review.clips.length; ++c) marks.push(Review.clips[c].start, Review.clips[c].end)
        marks.sort(function (a, b) { return a - b })
        var t = bar.playheadSec, target = direction > 0 ? marks[marks.length - 1] : marks[0]
        for (var i = 0; i < marks.length; ++i) {
            if (direction > 0 && marks[i] > t + 0.01) { target = marks[i]; break }
            if (direction < 0 && marks[i] < t - 0.01) target = marks[i]
        }
        player.pause(); seekTo(target)
    }
    function leave() {
        if (Review.unsaved && !confirmLeave) { confirmLeave = true; return }
        player.stop(); confirmLeave = false; Review.close()
    }
    function handleKey(e) {
        var ctrl = e.modifiers & Qt.ControlModifier, shift = e.modifiers & Qt.ShiftModifier, alt = e.modifiers & Qt.AltModifier
        if (e.key === Qt.Key_Space) togglePlayback()
        else if (e.key === Qt.Key_S && ctrl) Review.save()
        else if (e.key === Qt.Key_S) Review.split(bar.playheadSec)
        else if (e.key === Qt.Key_X || e.key === Qt.Key_Delete || e.key === Qt.Key_Backspace) cutOrRestore()
        else if (e.key === Qt.Key_Left) seekTo(bar.playheadSec - (shift ? 5 : alt ? 0.2 : 1))
        else if (e.key === Qt.Key_Right) seekTo(bar.playheadSec + (shift ? 5 : alt ? 0.2 : 1))
        else if (e.key === Qt.Key_BracketLeft) jumpToMark(-1)
        else if (e.key === Qt.Key_BracketRight) jumpToMark(1)
        else if (e.key === Qt.Key_Z && ctrl && shift) Review.redo()
        else if (e.key === Qt.Key_Y && ctrl) Review.redo()
        else if (e.key === Qt.Key_Z && ctrl) Review.undo()
        else if (e.key === Qt.Key_Z) bar.toggleZoom()
        else if (e.key === Qt.Key_Escape || e.key === Qt.Key_Return || e.key === Qt.Key_Enter) leave()
    }

    MediaPlayer {
        id: player
        source: Review.open ? Review.clip : ""
        videoOutput: video
        audioOutput: AudioOutput {}
        onPositionChanged: {
            if (playbackState === MediaPlayer.PlayingState) {
                var t = position / 1000, next = root.playableFrom(t)
                if (next < 0) { pause(); bar.playheadSec = Review.clips[Review.clips.length - 1].end; return }   // past the last clip: stop
                if (next - t > 0.05) { position = Math.round(next * 1000); return }                              // inside a cut: hop to the next clip
            }
            if (!bar.interacting) bar.playheadSec = position / 1000
        }
        property bool primed: false
        onSourceChanged: { bar.playheadSec = 0; primed = false }
        onMediaStatusChanged: {   // show the first kept frame instead of black until Play
            if (!primed && mediaStatus === MediaPlayer.LoadedMedia && Review.clips.length > 0) {
                primed = true
                pause(); root.seekTo(Review.clips[0].start)
            }
        }
    }

    Column {
        anchors.centerIn: parent
        width: root.videoWidth
        spacing: 14

        Row {
            spacing: 14
            Txt { text: Review.title; font.pixelSize: 14; font.bold: true; elide: Text.ElideMiddle; width: Math.min(implicitWidth, root.videoWidth * 0.6) }
            Txt {
                text: root.fmt(Review.duration) + " recorded, keeping " + root.fmt(Review.keptDuration)
                color: Omarchy.tertiaryText; font.pixelSize: 13
            }
        }

        Rectangle {
            width: parent.width; height: Math.round(width * 9 / 16)
            radius: 16; color: Omarchy.darkerBackground; border.color: Omarchy.selection; border.width: 1
            VideoOutput {
                id: video
                anchors { fill: parent; margins: 1 }
                fillMode: VideoOutput.PreserveAspectFit
                endOfStreamPolicy: VideoOutput.KeepLastFrame
            }
            Glyph {
                anchors.centerIn: parent
                visible: player.playbackState !== MediaPlayer.PlayingState
                text: ""; font.pixelSize: 54; color: Omarchy.foreground; opacity: 0.75
            }
            TapHandler { onTapped: root.togglePlayback() }
        }

        EditBar {
            id: bar
            width: parent.width
            height: 64
            accent: Omarchy.accent
            accentForeground: Omarchy.accentForeground
            cornerRadius: 12
            onScrub: function (seconds) { player.pause(); player.position = Math.round(seconds * 1000) }
        }

        Row {
            spacing: 14
            Rectangle {
                width: 40; height: 40; radius: 20; color: "transparent"; border.color: Omarchy.selection; border.width: 2
                Glyph { anchors.centerIn: parent; text: player.playbackState === MediaPlayer.PlayingState ? "" : ""; font.pixelSize: 15; color: Omarchy.foreground }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: root.togglePlayback() }
            }
            Txt { text: root.fmt(bar.playheadSec) + " / " + root.fmt(Review.duration); font.pixelSize: 13; color: Omarchy.secondaryText; anchors.verticalCenter: parent.verticalCenter }
            Item { width: 12; height: 1 }
            Txt { visible: Review.pauses.length > 0; text: Review.pauses.length + (Review.pauses.length === 1 ? " pause" : " pauses") + " marked"; font.pixelSize: 12; color: Omarchy.tertiaryText; anchors.verticalCenter: parent.verticalCenter }
        }

        Row {
            spacing: 12
            Button {
                text: Review.saving ? "saving " + Math.round(Review.saveProgress * 100) + "%" : "save edit"
                primary: true
                enabled: Review.changed && !Review.saving
                onClicked: Review.save()
            }
            Button {
                text: root.confirmLeave ? "leave without saving" : "done"
                tone: root.confirmLeave ? Omarchy.red : Omarchy.foreground
                enabled: !Review.saving
                onClicked: root.leave()
            }
            Button {
                text: root.confirmDelete ? "yes, move to trash" : "delete take"
                tone: Omarchy.red
                enabled: !Review.saving
                onClicked: { if (root.confirmDelete) { player.stop(); root.confirmDelete = false; Review.deleteTake() } else root.confirmDelete = true }
            }
            Button { visible: root.confirmDelete; text: "keep it"; onClicked: root.confirmDelete = false }
        }

        Txt {
            width: parent.width; wrapMode: Text.WordWrap
            visible: text !== ""
            text: Engine.warning !== "" ? Engine.warning : Review.message
            color: Engine.warning !== "" || Review.message.indexOf("could not") === 0 ? Omarchy.red : Omarchy.tertiaryText
            font.pixelSize: 12
        }
    }
}
