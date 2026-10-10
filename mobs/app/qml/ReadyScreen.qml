import QtQuick
import MobsCore

// One big record button, plain words, and what will be recorded.
Item {
    id: root
    readonly property bool showPreview: App.preview
    Column {
        id: readyColumn
        anchors.centerIn: parent
        anchors.verticalCenterOffset: showPreview ? -6 : root.height < 500 ? 0 : -20
        spacing: showPreview && Engine.micChecking ? 10 : showPreview ? 14 : root.height < 500 ? 16 : 22
        PreviewView {
            objectName: "readyPreview"
            visible: showPreview
            anchors.horizontalCenter: parent.horizontalCenter
            // Reserve the actual space used by controls, meters and explanations.
            // A fixed budget missed the extra hotkey row on taller windows.
            readonly property int extraRows: (hotkeys.visible ? 1 : 0) + (micMeter.visible ? 1 : 0) + (warningText.visible ? 1 : 0) + (noticeText.visible ? 1 : 0)
            readonly property real reservedHeight: recordButton.height + summary.height + chips.height
                + (hotkeys.visible ? hotkeys.height : 0) + (micMeter.visible ? micMeter.height : 0)
                + (warningText.visible ? warningText.height : 0) + (noticeText.visible ? noticeText.height : 0)
                + readyColumn.spacing * (3 + extraRows) + 12
            previewWidth: Math.min(480, Math.max(96, (root.height - reservedHeight - captionHeight) * 16 / 9))   // a small window with several rows shrinks the picture, never pushes it off the top
        }
        Row {
            id: startRow
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 28
            Item {
                id: recordButton
                objectName: "recordButton"
                enabled: !Review.loading
                opacity: Review.loading ? 0.4 : 1
                width: showPreview && Engine.micChecking ? 96 : showPreview || Engine.micChecking ? 112 : root.height < 500 ? 144 : 190; height: width
                Rectangle { anchors.fill: parent; radius: width / 2; color: "transparent"; border.color: Omarchy.accent; border.width: showPreview ? 3 : 4 }
                Rectangle { anchors.centerIn: parent; width: parent.width * 0.67; height: width; radius: width / 2; color: Omarchy.red }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: Engine.start(App.pick) }
            }
            // The broadcast has its own plain button beside Record: one click arms it and starts it. (The chip below arms it for Record or the
            // hotkey instead.) Without an address and key yet, it takes you to the page that asks for them.
            Rectangle {
                id: goLive
                objectName: "goLiveButton"
                signal clicked()
                enabled: !Review.loading
                y: (recordButton.height - height) / 2
                width: goLiveLabel.implicitWidth + 40; height: 46; radius: 23
                color: Omarchy.red   // always bold red, set up or not: it must be the first thing the eye finds. Without a key it opens the page that asks for one.
                border.width: 2; border.color: Omarchy.red
                Txt {
                    id: goLiveLabel
                    anchors.centerIn: parent
                    font.pixelSize: 15; font.bold: true
                    text: "GO LIVE to X"
                    color: Omarchy.accentForeground
                }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: goLive.clicked() }
                onClicked: { if (Prefs.streamReady) { Engine.setStreamArmed(true); Engine.start(App.pick) } else App.openLive() }
            }
        }
        Column {
            id: summary
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 6
            Txt { text: Review.loading ? "opening your recording…" : Engine.streamArmed ? "ready to go LIVE" : "ready to record"; color: Engine.streamArmed ? Omarchy.red : Omarchy.foreground; font.pixelSize: 24; font.bold: true; anchors.horizontalCenter: parent.horizontalCenter }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 10
                Txt {
                    text: App.pickName + (Prefs.micOn ? ", " + Audio.name(Prefs.micDevice) : ", mic off") + (Prefs.desktopOn ? ", desktop sound" : "")
                    width: Math.min(580, root.width - 170); horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                    color: Omarchy.secondaryText; font.pixelSize: 14
                }
                Rectangle {   // the way back to "what are we making?": my screen / screen + me / just my camera
                    width: changeLabel.implicitWidth + 22; height: 26; radius: 13
                    color: "transparent"
                    border.color: changeHover.hovered ? Omarchy.accent : Omarchy.selection; border.width: 1
                    Txt { id: changeLabel; anchors.centerIn: parent; text: "change"; font.pixelSize: 12; color: Omarchy.accent }
                    HoverHandler { id: changeHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: App.back() }
                }
            }
            Txt {
                objectName: "liveWarning"
                visible: Engine.streamArmed
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(580, root.width - 48); horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                text: "Record BROADCASTS to X: this screen is public until you stop."
                color: Omarchy.red; font.pixelSize: 13
            }
        }
        Row {
            id: hotkeys
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 8
            visible: !root.showPreview || root.height >= 600
            Key { text: "Alt" }
            Key { text: "Shift" }
            Key { text: "V" }
            Txt { text: "starts it from anywhere"; color: Omarchy.tertiaryText; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter; leftPadding: 8 }
        }
        LevelMeter { id: micMeter; visible: Engine.micChecking; width: 340; anchors.horizontalCenter: parent.horizontalCenter; db: Engine.micDb; receiving: Engine.micFrames }
        Flow {
            id: chips
            width: Math.min(root.width - 48, 820)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 12
            Chip { glyph: ""; text: App.displayText + "  (change)"; clickable: true; onClicked: Engine.forgetScreen(); visible: App.pick !== 3 }
            Chip { glyph: ""; text: Prefs.micOn ? "mic on" : "mic off"; tint: Prefs.micOn ? Omarchy.accent : Omarchy.tertiaryText; clickable: true; onClicked: Prefs.setMicOn(!Prefs.micOn) }
            Chip { glyph: ""; text: "EXPERT audio"; clickable: true; onClicked: App.openAudio() }
            Chip { glyph: ""; text: Prefs.desktopOn ? "desktop on" : (App.pick === 4 ? "desktop off: guest is silent" : "desktop off"); tint: App.pick === 4 && !Prefs.desktopOn ? Omarchy.red : Omarchy.accent; clickable: true; onClicked: Prefs.setDesktopOn(!Prefs.desktopOn) }
            Chip { glyph: "⚙"; text: Prefs.customQuality ? "custom quality" : ["small file", "balanced", "high quality"][Prefs.quality]; clickable: true; onClicked: App.setPro(true) }
            Chip {
                glyph: "\uf03d"
                clickable: true
                text: App.pick === 1 ? "camera off" : (App.hasCamera ? App.cameraName : "choose a camera")
                tint: App.pick === 1 || !App.hasCamera ? Omarchy.tertiaryText : Omarchy.accent
                onClicked: { App.refreshCameras(); App.setCameraPanel(true) }
            }
            Chip {
                glyph: "\uf06e"
                clickable: true
                text: App.preview ? "preview on" : "preview off"
                tint: App.preview ? Omarchy.accent : Omarchy.tertiaryText
                onClicked: App.setPreview(!App.preview)
            }
            Chip {
                objectName: "liveChip"
                glyph: "\uf2ce"
                clickable: true
                text: Engine.streamArmed ? "LIVE: armed" : "go live"
                tint: Engine.streamArmed ? Omarchy.red : Omarchy.tertiaryText
                // Arming is always a deliberate click. Without an address and key there is nothing to arm, so it opens the page that sets them.
                onClicked: { if (Engine.streamArmed) Engine.setStreamArmed(false); else if (Prefs.streamReady) Engine.setStreamArmed(true); else App.openLive() }
            }
        }
        Txt {
            id: warningText
            text: Engine.warning
            visible: Engine.warning !== ""
            color: Omarchy.red
            font.pixelSize: 13
            width: Math.min(640, root.width - 48)
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Txt {
            id: noticeText
            objectName: "readyNotice"
            text: [Engine.message, Engine.audioProblem, Engine.pictureProblem].filter((message, index, all) => message !== "" && all.indexOf(message) === index).join("\n")
            visible: Engine.audioProblem !== "" || Engine.message !== "" || Engine.pictureProblem !== ""
            width: Math.min(640, root.width - 48)
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: Engine.pictureProblem !== "" ? Omarchy.red : Omarchy.accent   // a black picture is the one notice that must not look routine
            font.pixelSize: 13
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }
    CameraPanel { anchors.fill: parent; z: 10 }
}
