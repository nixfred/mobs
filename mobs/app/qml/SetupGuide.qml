import QtQuick
import MobsCore

// The guided setup ("easy mode"): one thing per page, a big NEXT, and real pictures of what to click. It opens by itself the first time
// and any time from "setup guide" on the ready screen. It never asks anything MOBS can work out, and it keeps what the person chose:
// the camera and the microphone they lock in, the whole screen, and X (MOBS reads the stream key from X itself). Where the person stopped is remembered (Prefs.setupStep).
Rectangle {
    id: root
    objectName: "contentScreen"   // the layout check treats it like every other screen: nothing may leave its area
    color: Omarchy.background
    visible: App.setupOpen
    property int step: Math.max(0, Math.min(Prefs.setupStep, titles.length - 1))   // a step saved by an older build can never point past the last page
    readonly property var titles: ["welcome", "camera", "microphone", "your screen", "your webcam", "your X account", "going live", "all set"]
    readonly property int last: titles.length - 1
    readonly property var page: step === titles.indexOf("your X account") ? xPageLoader.item : pageLoader.item
    readonly property bool pageCanNext: page ? page.canNext : false
    function go(n) {
        const to = Math.max(0, Math.min(last, n))
        // The webcam page puts the webcam window up, and it keeps the camera open for the rest of the guide. Going back to the CAMERA page (or before it) must let go of it:
        // that page opens the same camera for its own picture, and a webcam allows one reader, so the picture would stay black. Only then: the microphone and screen pages
        // in between do not touch the camera, and the screen page needs the shared picture kept (its NEXT waits for a frame, which letting go would throw away).
        if (to <= titles.indexOf("camera")) App.setSetupPreview(false)
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
        sourceComponent: [welcome, camera, mic, screen, webcam, null, how, done][root.step]   // (the X page has a loader of its own, below)
    }
    LingerLoader {   // the X page holds a web browser: it is let go of a moment after the page is left, so no answer from X's page is still on its way when it goes
        id: xPageLoader
        anchors { top: progress.bottom; bottom: buttons.top; left: parent.left; right: parent.right; topMargin: 6; bottomMargin: 6 }
        wanted: root.step === root.titles.indexOf("your X account") && App.setupOpen
        visible: wanted
        sourceComponent: xAccount
    }
    Component { id: welcome; SetupWelcome {} }
    Component { id: camera; SetupCamera {} }
    Component { id: mic; SetupMic {} }
    Component { id: screen; SetupScreen {} }
    Component { id: webcam; SetupWebcam {} }
    Component { id: xAccount; SetupX {} }
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
