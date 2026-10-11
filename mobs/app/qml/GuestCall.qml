import QtQuick
import QtWebEngine
import MobsCore

// The call itself: a Jitsi room in a web view that fills the guest card above your webcam. Loaded only when a call starts (PipWindow's Loader), so
// the web engine costs nothing until a guest comes. The viewer opens the same room in their own browser; here you hear them (their voice plays like
// any other sound, so the desktop-sound capture puts it on the broadcast) and the card shows their picture.
//   Your camera is never offered to the page: the dock owns it, and a webcam can be open in only one program. Only the microphone is granted.
//   Nobody can share a screen into this call, and nothing in it can read your clipboard or open a plug-in.
//   The host sign-in (Jitsi wants a signed-in host to open a room) opens in a small dialog, and never while a take runs: viewers would watch you type.
Item {
    id: root
    objectName: "guestCall"
    // Jitsi lays itself out for a window about this wide; the card is smaller, so the page is shrunk to fit. While it still waits for a signed-in host its
    // dialog is tall (its "Log in" button sits low), so the page is laid out wider, which makes it shorter on the screen and the whole dialog visible.
    readonly property real layoutWidth: Call.joined ? 800 : 1100
    readonly property url callUrl: Call.url            // a QML property only signals when the address really changes: a status line never reloads the page

    WebEngineView {
        id: web
        objectName: "callWeb"
        anchors.fill: parent
        profile: profile
        url: root.callUrl
        zoomFactor: Math.max(0.25, Math.min(1, width / root.layoutWidth))
        backgroundColor: "#0e1522"
        settings.playbackRequiresUserGesture: false       // their voice must play without anybody clicking the page
        settings.screenCaptureEnabled: false
        settings.javascriptCanAccessClipboard: false
        settings.pluginsEnabled: false
        settings.fullScreenSupportEnabled: false
        userScripts.collection: [   // Qt 6: a script is a value, written as an object literal (it is not an element)
            {
                name: "mobs-call",
                sourceCode: Call.pageScript,
                injectionPoint: WebEngineScript.DocumentReady,
                worldId: WebEngineScript.MainWorld        // Jitsi's own page global is only visible in the page's world
            }
        ]
        onLoadingChanged: function (info) {
            if (info.status === WebEngineView.LoadSucceededStatus) Call.pageLoaded(true, "")
            else if (info.status === WebEngineView.LoadFailedStatus) Call.pageLoaded(false, info.errorString)
        }
        onJavaScriptConsoleMessage: function (level, message, lineNumber, sourceID) {
            if (message.indexOf("MOBS-CALL ") === 0) Call.report(message.substring(10))
            else if (Call.debug && message.indexOf("MOBS-CALL-DEBUG ") === 0) Call.pageSaid(message.substring(16))
            else if (Call.debug && level >= WebEngineView.WarningMessageLevel) Call.log("page console: " + message.substring(0, 160))
        }
        onPermissionRequested: function (permission) {
            const fromJitsi = ("" + permission.origin).indexOf("https://meet.jit.si") === 0
            if (fromJitsi && permission.permissionType === WebEnginePermission.PermissionType.MediaAudioCapture) { Call.log("the call may use the microphone"); permission.grant() }
            else { Call.log("the call page asked for something else (type " + permission.permissionType + "): refused, your camera stays with the dock"); permission.deny() }
        }
        onNavigationRequested: function (request) {
            const target = "" + request.url
            const testPage = Call.debug && target.startsWith("file:///tmp/")   // debugging only: a local page that reports its own clicks
            if (request.isMainFrame && !target.startsWith("https://") && target !== "about:blank" && !testPage) {
                Call.log("navigation refused: " + target.split(/[?#]/)[0].substring(0, 120))
                request.action = WebEngineNavigationRequest.IgnoreRequest
            }
        }
        onNewWindowRequested: function (request) {
            // Jitsi opens its sign-in window from a callback after the click, which Qt reports as not user-initiated, so the click cannot be required.
            // The only page this view ever shows is meet.jit.si; what matters is that it is https and that no take is running.
            const target = ("" + request.requestedUrl).split(/[?#]/)[0]
            Call.log("the page wants a window: " + target + (request.userInitiated ? " (after a click)" : ""))
            if (!target.startsWith("https://")) return
            if (!Call.signInAllowed()) return             // never while a take runs
            signIn.visible = true
            request.openIn(signInView)
        }
        onUrlChanged: Call.log("page address: " + ("" + url).split(/[?#]/)[0])   // never the part after ? or #: it can carry a sign-in token
    }

    Window {   // the host sign-in: a page that wants to open a window of its own gets this small dialog (it closes itself when the sign-in is done)
        id: signIn
        title: "mobs: sign in for the call"
        flags: Qt.Dialog
        transientParent: root.Window.window
        width: 520; height: 720
        color: "#10141c"
        WebEngineView {
            id: signInView
            anchors.fill: parent
            profile: profile
            onWindowCloseRequested: signIn.close()
            onNewWindowRequested: function (request) { request.openIn(signInView) }
            onPermissionRequested: function (permission) { permission.deny() }
        }
    }

    // Declared last on purpose: the views above are deleted first, and a web profile must outlive every page that uses it.
    WebEngineProfile {
        id: profile
        storageName: "mobs-call"                          // its own cookies: your host sign-in lives here, apart from every other browser
        offTheRecord: false
        persistentCookiesPolicy: WebEngineProfile.ForcePersistentCookies
    }
}
