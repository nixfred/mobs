import QtQuick
import MobsCore

// The guided setup ("easy mode"): one thing per page, a big NEXT, and real pictures of what to click. It opens by itself the first time
// and any time from "setup guide" on the ready screen. It never asks anything MOBS can work out, and it keeps what the person chose:
// the camera and the microphone they lock in, the whole screen, the X stream key. Where the person stopped is remembered (Prefs.setupStep).
Rectangle {
    id: root
    objectName: "contentScreen"   // the layout check treats it like every other screen: nothing may leave its area
    color: Omarchy.background
    visible: App.setupOpen
    property int step: Math.max(0, Math.min(Prefs.setupStep, titles.length - 1))   // a step saved by an older build can never point past the last page
    readonly property var titles: ["welcome", "camera", "microphone", "your screen", "your webcam", "open X", "make a source", "stream key", "chat", "going live", "all set"]
    readonly property int last: titles.length - 1
    readonly property var page: pageLoader.item
    readonly property bool pageCanNext: page ? page.canNext : false
    function go(n) {
        const to = Math.max(0, Math.min(last, n))
        if (to !== step) Prefs.setSetupStep(to)
    }
    function next() {
        if (!pageCanNext) return
        if (step >= last) finish(); else go(step + 1)
    }
    function back() { go(step - 1) }
    function skip() {
        if (page && page.canSkip) { if (page.skipNow) page.skipNow(); go(step + 1) }
    }
    function finish() { App.setSetupPreview(false); Prefs.setSetupDone(true); Prefs.setSetupStep(0); App.closeSetup() }
    function handleKey(e) {
        if (e.key === Qt.Key_Return || e.key === Qt.Key_Enter || e.key === Qt.Key_Space) next()
        else if (e.key === Qt.Key_Escape || e.key === Qt.Key_Backspace || e.key === Qt.Key_Left) back()
    }

    // where we are: dots, and the name of this step
    Item {
        id: progress
        anchors { top: parent.top; left: parent.left; right: parent.right; topMargin: 14 }
        height: 44
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 9
            Repeater {
                model: root.titles.length
                Rectangle {
                    required property int index
                    width: index === root.step ? 26 : 10; height: 10; radius: 5
                    color: index <= root.step ? Omarchy.accent : Omarchy.selection
                    Behavior on width { NumberAnimation { duration: 120 } }
                }
            }
        }
        Txt {
            objectName: "setupStepName"
            anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom }
            text: "step " + (root.step + 1) + " of " + root.titles.length + ": " + root.titles[root.step]
            font.pixelSize: 13; color: Omarchy.tertiaryText
        }
    }

    Txt {   // for the person who knows what they are doing: leave the guide, and it does not come back by itself
        objectName: "setupSkipAll"
        visible: root.step < root.last
        anchors { right: parent.right; rightMargin: 28; top: parent.top; topMargin: 18 }
        text: "skip the guide"; font.pixelSize: 12; font.underline: true; color: Omarchy.tertiaryText
        signal clicked()
        HoverHandler { cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: parent.clicked() }
        onClicked: root.finish()
    }

    Loader {
        id: pageLoader
        anchors { top: progress.bottom; bottom: buttons.top; left: parent.left; right: parent.right; topMargin: 6; bottomMargin: 6 }
        sourceComponent: [welcome, camera, mic, screen, webcam, studio, source, key, chatPage, how, done][root.step]
    }
    Component { id: welcome; SetupWelcome {} }
    Component { id: camera; SetupCamera {} }
    Component { id: mic; SetupMic {} }
    Component { id: screen; SetupScreen {} }
    Component { id: webcam; SetupWebcam {} }
    Component { id: studio; SetupStudio {} }
    Component { id: source; SetupSource {} }
    Component { id: key; SetupKey {} }
    Component { id: chatPage; SetupChat {} }
    Component { id: how; SetupHow {} }
    Component { id: done; SetupDone {} }

    Item {
        id: buttons
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right; bottomMargin: 14; leftMargin: 28; rightMargin: 28 }
        height: 56
        BigButton {
            objectName: "setupBack"
            anchors { left: parent.left; verticalCenter: parent.verticalCenter }
            primary: false; text: "BACK"; implicitWidth: 130; implicitHeight: 48
            visible: root.step > 0
            onClicked: root.back()
        }
        BigButton {
            objectName: "setupSkip"
            anchors { right: nextButton.left; rightMargin: 16; verticalCenter: parent.verticalCenter }
            primary: false; implicitHeight: 48
            text: root.page && root.page.skipText ? root.page.skipText : "skip"
            visible: root.page ? root.page.canSkip : false
            onClicked: root.skip()
        }
        BigButton {
            id: nextButton
            objectName: "setupNext"
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            text: root.step >= root.last ? "FINISH" : (root.page && root.page.nextText ? root.page.nextText : "NEXT")
            enabled: root.pageCanNext
            onClicked: root.next()
        }
    }
}
