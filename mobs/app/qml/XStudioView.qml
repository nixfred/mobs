import QtQuick
import QtWebEngine
import MobsCore

// A browser view of X Live Studio for MOBS's helper. It is MOBS's own browser profile (so it is signed in as the person, once), it only ever goes to x.com pages, and every page it
// loads gets the page half of the helper (assets/pilot/xstudio-pilot.js). ask(job, argument, done) gives the page one small job and calls done with the page's answer as an object
// ({} when the page could not answer; { missing: true } when the helper is not in it yet). The stream key can pass through an answer, so nothing here ever prints one.
WebEngineView {
    id: web
    readonly property string helper: pilotHelperJs   // the page half of the helper: a file in the app's resources, read once at start (main.cpp)
    property int seq: 0
    // A question whose answer never comes (the page's process died) must not hold the helpers for ever: after 15 s it is given up with an error answer, so their own clocks run and say so.
    Timer {
        id: askWatchdog
        interval: 15000
        property var giveUp: null
        onTriggered: { if (web.busy) { web.seq++; web.busy = false; const f = giveUp; giveUp = null; if (f) f({ error: "the page did not answer" }) } }
    }
    property bool busy: false                  // a question is out and its answer has not come back
    // The interface is drawn scaled up (Main.qml, uiScale). A web page inside it would be a small picture stretched, so it is drawn at the real size of its box in screen pixels and
    // shrunk back (scale 1 / k): sharp at any size. The page is then zoomed so that the form it shows (about 960 by 520) fills the box; a sign-in form (full) is used at the size of the
    // rest of the interface instead.
    property Item box: null                    // the box this fills (a Loader)
    property bool full: false
    readonly property real k: Window.window && Window.window.uiScale ? Window.window.uiScale : 1
    width: box ? box.width * k : 100
    height: box ? box.height * k : 100
    scale: 1 / k
    transformOrigin: Item.TopLeft
    zoomFactor: full ? k : Math.max(0.5, Math.min(3, width / 960, height / 520))
    readonly property bool testPage: xStudioUrl.indexOf("file://") === 0   // a stand-in page, only when a test points the address at a file
    profile: xProfile
    backgroundColor: "#000000"
    audioMuted: true                           // a page of X may play sound: it must never reach the desktop sound that goes on air
    settings.javascriptCanOpenWindows: false
    settings.javascriptCanAccessClipboard: false
    settings.screenCaptureEnabled: false
    settings.fullScreenSupportEnabled: false
    settings.pluginsEnabled: false
    settings.focusOnNavigationEnabled: false
    onWebAuthUxRequested: function (request) { request.cancel() }
    // X's sign-in page asks the browser for a passkey the moment it loads, and a small embedded browser has none to give: the page hangs. So it is told, before any of its own
    // code runs, that passkeys do not exist here (the same as the sign-in page of the setup guide), and goes on to the email and password way in.
    readonly property string noPasskeys: [
        "(function () {",
        "  try { delete window.PublicKeyCredential } catch (e) {}",
        "  try { Object.defineProperty(window, 'PublicKeyCredential', { value: undefined, configurable: true }) } catch (e) {}",
        "  var refuse = function () { return Promise.reject(new DOMException('Passkeys are not available in this window.', 'NotAllowedError')) };",
        "  try { if (navigator.credentials) { navigator.credentials.get = refuse; navigator.credentials.create = refuse } } catch (e) {}",
        "})()"
    ].join("\n")
    userScripts.collection: [ { name: "no passkeys", sourceCode: web.noPasskeys, injectionPoint: WebEngineScript.DocumentCreation, worldId: WebEngineScript.MainWorld, runsOnSubFrames: true } ]
    // https only, a plain host name (no user name before it, no other port), and it is x.com or twitter.com or a subdomain of one of them.
    function onXHost(target) {
        const m = /^https:\/\/([A-Za-z0-9.-]+)(?::443)?(?:[\/?#]|$)/.exec(target)
        return m !== null && ["x.com", "twitter.com"].some(function (d) { return m[1] === d || m[1].endsWith("." + d) })
    }
    onNavigationRequested: function (request) {
        const target = request.url.toString()
        if (!(onXHost(target) || (testPage && target.indexOf("file://") === 0) || target === "about:blank")) request.action = WebEngineNavigationRequest.IgnoreRequest
    }
    function inject() { if (helper !== "") web.runJavaScript(helper, WebEngineScript.MainWorld) }
    onLoadingChanged: function (info) { if (info.status === WebEngineView.LoadSucceededStatus) inject() }
    function ask(job, argument, done) {
        if (busy) return false
        busy = true
        const mine = ++seq
        askWatchdog.giveUp = done
        askWatchdog.restart()
        const call = "(function () { try { return window.__mobsPilot ? window.__mobsPilot." + job + "(" + (argument === undefined ? "" : JSON.stringify(argument)) + ") : '{\"missing\":true}' } catch (e) { return JSON.stringify({ error: String(e) }) } })()"
        web.runJavaScript(call, WebEngineScript.MainWorld, function (result) {
            if (mine !== seq) return                // the watchdog already gave this question up and told the caller
            askWatchdog.stop()
            busy = false
            let answer = {}
            try { answer = JSON.parse(result) } catch (e) { answer = {} }
            done(answer || {})
        })
        return true
    }
}
