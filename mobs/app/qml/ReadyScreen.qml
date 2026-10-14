import QtQuick
import MobsCore

// The ready screen in easy mode: what viewers will see, ONE big GO LIVE button, and a row of check marks that say what is set up. Anything not set
// up yet is a red mark that takes the person straight to the page of the guided setup that fixes it. Everything else lives in EXPERT.
Item {
    id: root
    readonly property bool showPreview: App.preview
    function guide(step) { Prefs.setSetupStep(step); App.openSetup() }
    readonly property bool cameraOk: App.pick === 1 || (App.cameraLocked && App.hasCamera)
    readonly property bool micOk: !Prefs.micOn || (Prefs.micLocked && Engine.audioProblem === "")
    Column {
        id: readyColumn
        anchors.centerIn: parent
        anchors.verticalCenterOffset: showPreview ? -4 : root.height < 500 ? 0 : -16
        spacing: showPreview ? 10 : root.height < 500 ? 14 : 20
        PreviewView {
            objectName: "readyPreview"
            visible: showPreview
            anchors.horizontalCenter: parent.horizontalCenter
            // The space the controls below really take: a fixed budget missed the extra rows on taller windows.
            readonly property real reservedHeight: goLive.height + liveOptions.height + statusLine.height + checks.height + recordOnly.height + (hotkeys.visible ? hotkeys.height : 0)
                + (warningText.visible ? warningText.height : 0) + (noticeText.visible ? noticeText.height : 0) + readyColumn.spacing * (5 + (hotkeys.visible ? 1 : 0)
                + (warningText.visible ? 1 : 0) + (noticeText.visible ? 1 : 0)) + 12
            previewWidth: Math.min(480, Math.max(96, (root.height - reservedHeight - captionHeight) * 16 / 9))   // a small window shrinks the picture, never pushes it off the top
        }
        // The one big button. Without an X key yet it takes the person to the guide instead of failing.
        Rectangle {
            id: goLive
            objectName: "goLiveButton"
            signal clicked()
            anchors.horizontalCenter: parent.horizontalCenter
            enabled: !Review.loading
            opacity: Review.loading ? 0.4 : 1
            width: Math.max(280, goLiveLabel.implicitWidth + 80); height: root.height < 520 ? 56 : 68; radius: height / 2
            color: Omarchy.red
            Txt { id: goLiveLabel; anchors.centerIn: parent; font.pixelSize: 22; font.bold: true; text: "GO LIVE to X"; color: Omarchy.accentForeground }
            HoverHandler { cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: goLive.clicked() }
            onClicked: { Window.window.focusKeys(); if (Prefs.streamReady) { Engine.setStreamArmed(true); Engine.start(App.pick) } else root.guide(5) }
        }
        // What the livestream is called, and who can watch it. X's Live Studio will not let anyone go live without a title, so MOBS types it there for you; both are kept until you change them.
        Row {
            id: liveOptions
            objectName: "liveOptions"
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 10
            Rectangle {
                width: Math.min(340, root.width - 200); height: 36; radius: 18; color: Omarchy.lighterBackground
                border.color: titleInput.activeFocus ? Omarchy.accent : Omarchy.selection
                TextInput {
                    id: titleInput
                    objectName: "liveTitleInput"
                    anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                    verticalAlignment: TextInput.AlignVCenter; clip: true; maximumLength: 100
                    font.pixelSize: 13; color: Omarchy.foreground
                    Component.onCompleted: text = Prefs.liveTitle
                    onTextEdited: Prefs.setLiveTitle(text)
                    onAccepted: Window.window.focusKeys()
                    Keys.onEscapePressed: Window.window.focusKeys()
                }
                Txt { anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter } visible: titleInput.text === ""; text: "title of your livestream: " + Prefs.liveTitleForX; font.pixelSize: 13; color: Omarchy.tertiaryText }
            }
            Chip {
                objectName: "livePublicChip"; glyph: ""; clickable: true; implicitHeight: 36
                text: Prefs.livePublic ? "everyone can watch" : "only me (test)"
                tint: Prefs.livePublic ? Omarchy.green : Omarchy.yellow
                onClicked: Prefs.setLivePublic(!Prefs.livePublic)
            }
        }
        Column {
            id: statusLine
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 4
            Txt {
                objectName: "readyStatus"
                anchors.horizontalCenter: parent.horizontalCenter
                font.pixelSize: 15
                color: Review.loading || Prefs.streamReady ? Omarchy.secondaryText : Omarchy.red
                text: Review.loading ? "opening your recording…" : Engine.streamArmed ? "ready to go LIVE" : Prefs.streamReady ? "everything is ready" : "one more step: connect X, then you can go live"
            }
            Txt {
                objectName: "liveWarning"
                visible: Engine.streamArmed
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(580, root.width - 48); horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                text: "GO LIVE broadcasts to X: this screen is public until you stop."
                color: Omarchy.red; font.pixelSize: 13
            }
        }
        // What is set up. Each one is a button: green means it is done, red takes you to the page that fixes it.
        Flow {
            id: checks
            width: Math.min(root.width - 48, 780)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 10
            Chip {
                objectName: "checkCamera"; glyph: ""; clickable: true; tint: root.cameraOk ? Omarchy.green : Omarchy.red
                text: App.pick === 1 ? "no camera" : App.hasCamera ? App.cameraName : App.cameraLocked ? App.lockedCameraName + " is unplugged" : "choose a camera"
                onClicked: root.guide(1)
            }
            Chip {
                objectName: "checkMic"; glyph: ""; clickable: true; tint: root.micOk ? Omarchy.green : Omarchy.red
                text: Prefs.micOn ? Audio.name(Prefs.micDevice) : "no microphone"
                onClicked: root.guide(2)
            }
            Chip {
                objectName: "checkScreen"; glyph: ""; clickable: true; tint: Engine.pictureProblem !== "" ? Omarchy.red : Preview.hasFrame ? Omarchy.green : Omarchy.yellow
                text: Engine.pictureProblem !== "" ? "screen is black: fix it" : Preview.hasFrame ? "whole screen" : "share your screen"
                onClicked: root.guide(3)
            }
            Chip {
                objectName: "checkX"; glyph: "X"; clickable: true; tint: Prefs.streamReady ? Omarchy.green : Omarchy.red
                text: Prefs.streamReady ? (Prefs.sourceName !== "" ? "X: " + Prefs.sourceName : "X connected") : "connect X"
                onClicked: root.guide(5)
            }
            Chip {
                objectName: "checkChat"; glyph: "\uf086"; clickable: true
                tint: Chat.xId !== "" ? Omarchy.green : Prefs.xSignedIn ? Omarchy.yellow : Omarchy.tertiaryText
                text: Chat.xId !== "" ? "chat attached" : Prefs.xSignedIn ? "chat: found when you go live" : "add chat"
                onClicked: root.guide(5)
            }
        }
        Row {
            id: recordOnly
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 12
            Rectangle {   // a plain recording: nothing goes to X
                id: recordButton
                objectName: "recordButton"
                enabled: !Review.loading
                opacity: Review.loading ? 0.4 : 1
                width: recordLabel.implicitWidth + 36; height: 38; radius: 19; color: "transparent"
                border.width: 1; border.color: recordHover.hovered ? Omarchy.accent : Omarchy.selection
                Txt { id: recordLabel; anchors.centerIn: parent; text: "just record, nothing goes to X"; font.pixelSize: 13; color: Omarchy.secondaryText }
                HoverHandler { id: recordHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: Engine.start(App.pick) }
            }
            Chip {
                objectName: "liveChip"; glyph: ""; clickable: true; visible: Prefs.streamReady
                text: Engine.streamArmed ? "hotkey goes LIVE" : "hotkey records"
                tint: Engine.streamArmed ? Omarchy.red : Omarchy.tertiaryText
                implicitHeight: 38
                onClicked: Engine.setStreamArmed(!Engine.streamArmed)
            }
            Chip { objectName: "moreChip"; glyph: "⚙"; clickable: true; text: "more options"; implicitHeight: 38; onClicked: App.setPro(true) }
        }
        Row {
            id: hotkeys
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 8
            visible: !root.showPreview || root.height >= 640
            Key { text: "Super" }
            Key { text: "Shift" }
            Key { text: "V" }
            Txt { text: "starts and stops it from any app"; color: Omarchy.tertiaryText; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter; leftPadding: 8 }
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
