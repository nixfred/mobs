import QtQuick
import QtWebEngine
import MobsCore

// The MOBS window. Screens so far: choose and ready. Everything follows the live Omarchy theme.
Window {
    id: win
    visible: true
    // The size it opens at when the window is floating (a tiled window takes whatever its tile is): big, and never more than 90% of the screen.
    width: Math.max(860, Math.min(1500, Screen.desktopAvailableWidth * 0.9))
    height: Math.max(560, Math.min(940, Screen.desktopAvailableHeight * 0.9))
    minimumWidth: 860
    minimumHeight: 560
    // How much the interface is scaled up: as much as the window allows without the canvas getting smaller than 860 by 560 (1 at the smallest window, 1.7 at 1500 by 940, 3 at most).
    readonly property real uiScale: Math.max(1, Math.min(3, Math.min(width / 860, height / 560)))
    title: "mobs"
    color: Omarchy.background

    // A text box on a screen (the livestream title on the ready screen) lets go of the keyboard when it is done, so the keys of the screen work again.
    function focusKeys() { keyboard.forceActiveFocus() }
    // Monologue's unchanged EditBar calls these methods on its host window.
    function preciseTime(seconds) { return reviewScreen.preciseTime(seconds) }
    function restoreGap(gap) { reviewScreen.restoreGap(gap) }

    property bool closeAfterRecording: false
    property bool confirmQuit: false
    property bool confirmStopQuit: false
    onConfirmQuitChanged: if (confirmQuit) keyboard.forceActiveFocus()
    onConfirmStopQuitChanged: if (confirmStopQuit) keyboard.forceActiveFocus()
    onClosing: (close) => {
        if (Engine.active) {
            close.accepted = false
            if (closeAfterRecording && Engine.state === "stopping") confirmStopQuit = true
            closeAfterRecording = true
            Pip.kill()      // a stop of a broadcast ends the livestream on X too (the window then waits for X's answer before it closes)
        } else if (Review.saving || Review.unsaved) {
            close.accepted = false
            confirmQuit = true
        } else if (Review.loading) Review.close()
    }
    Connections {   // the take is over and the window was asked to close, but the livestream on X was still being ended: now it is done
        target: Chat
        function onEndingChanged() {
            if (!Chat.ending && win.closeAfterRecording && !Engine.active) {
                win.closeAfterRecording = false
                if (Engine.message.indexOf("saved ") === 0) win.close()
            }
        }
    }
    Connections {
        target: Engine
        function onStateChanged() {
            if (!Engine.active) win.confirmStopQuit = false
            if (win.closeAfterRecording && !Engine.active && !Chat.ending) {
                win.closeAfterRecording = false
                // Failed/cancelled recordings remain visible with their explanation.
                if (Engine.message.indexOf("saved ") === 0) win.close()
            }
        }
    }

    // EASY MODE: the whole interface is drawn on a canvas that is never smaller than 860 by 560 and is then scaled up to fill the window, so on a big or tiled window
    // every word, button and picture grows with it (a 5120 pixel wide screen got 13 pixel text before this). Nothing is cut off and nothing scrolls, because the canvas
    // keeps the shape of the window and is never smaller than the size every screen was made to fit. The webcam window is a window of its own and is not scaled.
    Item {
        id: ui
        objectName: "ui"
        width: win.width / win.uiScale
        height: win.height / win.uiScale
        scale: win.uiScale
        transformOrigin: Item.TopLeft
        Header {
            enabled: !win.confirmQuit && !win.confirmStopQuit
            id: header
            anchors { top: parent.top; left: parent.left; right: parent.right }
            stateWord: App.setupOpen ? "setup" : App.screen === 1 ? "new" : App.screen === 3 ? (Engine.broadcasting && Engine.state === "recording" ? "LIVE" : Engine.state) : App.screen === 4 ? "review" : "ready"
        }
        ChooseScreen {
            objectName: "contentScreen"
            visible: App.screen === 1 && !App.pro
            anchors { top: header.bottom; bottom: footer.top; left: parent.left; right: parent.right }
        }
        ReadyScreen {
            objectName: "contentScreen"
            visible: App.screen === 2 && !App.pro
            anchors { top: header.bottom; bottom: footer.top; left: parent.left; right: parent.right }
        }
        RecordingScreen {
            objectName: "contentScreen"
            visible: App.screen === 3 && !App.pro
            anchors { top: header.bottom; bottom: footer.top; left: parent.left; right: parent.right }
        }
        ReviewScreen {
            objectName: "contentScreen"
            id: reviewScreen
            visible: App.screen === 4 && !App.pro
            anchors { top: header.bottom; bottom: footer.top; left: parent.left; right: parent.right }
        }
        ProDrawer {
            enabled: !win.confirmQuit && !win.confirmStopQuit
            id: proDrawer
            anchors { top: header.bottom; bottom: footer.top; left: parent.left; right: parent.right }
            z: 20
        }
        Footer {
            id: footer
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            Row { spacing: 8; visible: App.pro; Key { text: "Esc" } Txt { text: "close EXPERT"; color: Omarchy.tertiaryText; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
            Row { spacing: 8; visible: !App.pro && App.screen === 1; Key { text: "1" } Key { text: "2" } Key { text: "3" } Txt { text: "pick"; color: Omarchy.tertiaryText; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
            Row { spacing: 8; visible: !App.pro && App.screen === 2; Key { text: "Space" } Txt { text: "record"; color: Omarchy.tertiaryText; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
            Row { spacing: 8; visible: !App.pro && App.screen === 3; Key { text: "Space" } Txt { text: "pause / resume"; color: Omarchy.tertiaryText; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
            Row { spacing: 8; visible: !App.pro && App.screen === 3; Key { text: "Enter" } Txt { text: "stop and save"; color: Omarchy.tertiaryText; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
            Row { spacing: 8; visible: !App.pro && App.screen === 4; Key { text: "Space" } Txt { text: "play"; color: Omarchy.tertiaryText; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
            Row { spacing: 8; visible: !App.pro && App.screen === 4; Key { text: "S" } Txt { text: "split"; color: Omarchy.tertiaryText; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
            Row { spacing: 8; visible: !App.pro && App.screen === 4; Key { text: "X" } Txt { text: "cut"; color: Omarchy.tertiaryText; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
            Row { spacing: 8; visible: !App.pro && App.screen === 4; Key { text: "Ctrl" } Key { text: "S" } Txt { text: "save"; color: Omarchy.tertiaryText; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
            Row { spacing: 8; visible: !App.pro && App.screen === 4; Key { text: "Esc" } Txt { text: "done"; color: Omarchy.tertiaryText; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
            Row { spacing: 8; visible: !App.pro && App.screen === 2; Key { text: "P" } Txt { text: "EXPERT"; color: Omarchy.tertiaryText; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
        }

        Rectangle {
            anchors.fill: parent; z: 100; visible: win.confirmQuit
            color: Omarchy.darkBackground
            TapHandler {}
            Column {
                anchors.centerIn: parent; width: Math.min(700, ui.width - 64); spacing: 22
                Txt { width: parent.width; wrapMode: Text.WordWrap; text: Review.saving ? "your edit is still saving" : "your edit has unsaved changes"; font.pixelSize: 22; font.bold: true }
                Txt { width: parent.width; wrapMode: Text.WordWrap; text: Review.saving ? "Keep MOBS open until the export finishes." : "Save the edit before quitting, or discard the edits. Your original recording stays on disk."; font.pixelSize: 14; color: Omarchy.secondaryText }
                Flow {
                    width: parent.width; spacing: 12
                    Button { text: "keep editing"; primary: true; onClicked: win.confirmQuit = false }
                    Button { text: "save edit"; visible: !Review.saving; onClicked: { win.confirmQuit = false; Review.save() } }
                    Button { text: "discard edits and quit"; visible: !Review.saving; tone: Omarchy.red; onClicked: { Review.close(); win.confirmQuit = false; win.close() } }
                }
            }
        }
        Connections {
            target: App
            function onProChanged() { keyboard.forceActiveFocus() }
        }
        Rectangle {
            anchors.fill: parent; z: 100; visible: win.confirmStopQuit
            color: Omarchy.darkBackground
            TapHandler {}
            Column {
                anchors.centerIn: parent; width: Math.min(700, ui.width - 64); spacing: 22
                Txt { width: parent.width; wrapMode: Text.WordWrap; text: "your recording is still saving"; font.pixelSize: 22; font.bold: true }
                Txt { width: parent.width; wrapMode: Text.WordWrap; text: "Keep waiting to finish the recording. If saving has stalled, you can quit now, but this recording may be incomplete or unplayable."; font.pixelSize: 14; color: Omarchy.secondaryText }
                Flow {
                    width: parent.width; spacing: 12
                    Button { text: "keep waiting"; primary: true; onClicked: win.confirmStopQuit = false }
                    Button { objectName: "forceQuitButton"; text: "quit without waiting"; tone: Omarchy.red; onClicked: Engine.requestForceQuit() }
                }
            }
        }
        SetupGuide {   // the guided setup covers everything under the header, the footer included: its own keys are NEXT and BACK
            id: setupGuide
            anchors { top: header.bottom; bottom: parent.bottom; left: parent.left; right: parent.right }
            z: 30
        }
        Item {
            id: keyboard
            focus: true
            Keys.onPressed: (e) => {
                if (!splash.done) { splash.skip(); e.accepted = true; return }   // the opening screen eats the first key, so it can never start a recording
                if (win.confirmStopQuit) { if (e.key === Qt.Key_Escape) win.confirmStopQuit = false; e.accepted = true; return }
                if (win.confirmQuit) { if (e.key === Qt.Key_Escape) win.confirmQuit = false; e.accepted = true; return }
                if (App.setupOpen) { setupGuide.handleKey(e); e.accepted = true; return }
                if (App.pro) {
                    if (e.key === Qt.Key_Escape || e.key === Qt.Key_P) App.setPro(false)
                    e.accepted = true
                    return
                }
                if (App.cameraPanel) {
                    if (e.key === Qt.Key_Escape) App.setCameraPanel(false)
                    e.accepted = true
                    return
                }
                if (e.key === Qt.Key_P && App.screen !== 3 && !(e.modifiers & Qt.ControlModifier)) App.setPro(!App.pro)
                else if (e.key === Qt.Key_Escape && App.pro) App.setPro(false)
                else if (App.screen === 1 && e.key >= Qt.Key_1 && e.key <= Qt.Key_3) App.choose(e.key - Qt.Key_0)
                else if (App.screen === 2 && e.key === Qt.Key_Space) Engine.start(App.pick)
                else if (App.screen === 3 && e.key === Qt.Key_Space) Engine.togglePause()
                else if (App.screen === 3 && (e.key === Qt.Key_Return || e.key === Qt.Key_Enter)) Pip.kill()
                else if (App.screen === 4) reviewScreen.handleKey(e)
                else if (App.screen === 2 && e.key === Qt.Key_Escape) { if (App.cameraPanel) App.setCameraPanel(false) }   // easy mode has no "what are we making" page: screen + me, your whole screen
            }
        }
        SplashScreen { id: splash }   // on top of everything, scaled with the rest of the interface
    }
    PipWindow { id: pipWindow }
    // MOBS's own browser profile for X: the person signs in once (setup guide) and the chat card reuses it. Declared last on purpose: the web pages that
    // use it are deleted first, and a profile must outlive them. Its folder is MOBS_WEB_DIR in tests, so a test never writes into a real profile.
    WebEngineProfile {
        id: xProfile
        storageName: "mobs-x"
        offTheRecord: false
        persistentStoragePath: xStoragePath
        persistentCookiesPolicy: WebEngineProfile.ForcePersistentCookies
    }
}
