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
    property string studio: "idle"        // idle, checking, ok or failed: does Live Studio open for this account?
    property int tries: 0
    property url target: xLoginUrl
    function startCheck() { if (studio === "idle") { studio = "checking"; tries = 0; target = xStudioUrl } }
    Component.onCompleted: if (Prefs.xSignedIn) startCheck()
    Connections { target: Prefs; function onXSignedInChanged() { if (Prefs.xSignedIn) root.startCheck() } }
    Timer {   // Live Studio is a page of X's own: it counts as open when its own words are on it
        interval: 1500; repeat: true; running: root.studio === "checking" && !web.loading
        onTriggered: {
            root.tries++
            web.runJavaScript("document.body ? /livestreams/i.test(document.body.innerText) && /sources/i.test(document.body.innerText) : false", function (found) {
                if (found === true) root.studio = "ok"
                else if (root.tries >= xChatTries * 4) root.studio = "failed"   // about a minute and a half, long enough to sign in again if Live Studio asks
            })
        }
    }
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
            color: Prefs.xSignedIn && root.studio !== "failed" ? Omarchy.green : Omarchy.tertiaryText
            width: parent.width; wrapMode: Text.WordWrap
            text: !Prefs.xSignedIn ? "Sign in below. When you see your X home page you are done."
                : root.studio === "ok" ? "Signed in, and Live Studio opens for your account. Press NEXT."
                : root.studio === "failed" ? "Signed in, but Live Studio did not open here (it needs X Premium). You can still go live; the chat just will not show. Press NEXT."
                : "Signed in. Checking that Live Studio opens for your account..."
        }
        Rectangle {
            width: parent.width; height: parent.height - y; radius: 10; color: "black"; clip: true
            border.color: Prefs.xSignedIn ? Omarchy.green : Omarchy.selection; border.width: 2
            WebEngineView {
                id: web
                anchors { fill: parent; margins: 2 }
                profile: xProfile
                url: root.target
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
