import QtQuick
import QtWebEngine
import MobsCore

// Step 9: sign in to X, once, in MOBS's own little browser. That is what lets the chat of the livestream show under the webcam: X only shows a
// livestream's chat to the person who owns it, so MOBS needs to be signed in as them. The password is typed into X's own page; MOBS never sees it.
Item {
    id: root
    objectName: "setupChat"
    readonly property bool canNext: Prefs.xSignedIn
    readonly property bool canSkip: true
    readonly property string skipText: "skip: no chat in the picture"
    Column {
        anchors { fill: parent; leftMargin: 28; rightMargin: 28 }
        spacing: 8
        Txt { text: "Chat under your webcam"; font.pixelSize: 24; font.bold: true }
        Txt {
            width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 13; color: Omarchy.secondaryText
            text: "Sign in to X here, once. Then your viewers' chat shows under your webcam and moves with it. You answer out loud, so there is nothing to type."
        }
        Txt {
            objectName: "setupChatStatus"
            font.pixelSize: 13
            color: Prefs.xSignedIn ? Omarchy.green : Omarchy.tertiaryText
            text: Prefs.xSignedIn ? "Signed in. Press NEXT." : "Sign in below. When you see your X home page you are done."
        }
        Rectangle {
            width: parent.width; height: parent.height - y; radius: 10; color: "black"; clip: true
            border.color: Prefs.xSignedIn ? Omarchy.green : Omarchy.selection; border.width: 2
            WebEngineView {
                anchors { fill: parent; margins: 2 }
                profile: xProfile
                url: xLoginUrl
                backgroundColor: "#000000"
                settings.javascriptCanOpenWindows: false
                settings.screenCaptureEnabled: false
                settings.fullScreenSupportEnabled: false
                settings.pluginsEnabled: false
                onUrlChanged: Prefs.noteXUrl(url.toString())
            }
        }
    }
}
