import QtQuick
import QtWebEngine
import MobsCore

// While a broadcast is on air and the person is signed in to X, MOBS finds their livestream BY ITSELF, so nobody has to copy a link. Live Studio's list page
// shows the livestream that is on air under "Live Now", with a plain link to it (/live/<id>). The page is loaded in a browser view of 1400 by 900 pixels whose
// parent is only 2 pixels wide and sits behind the picture of the webcam: it is showing, so X draws it, but nobody can see it. Nothing on the page is changed;
// MOBS reads links from it, and the chat card then opens that livestream's chat.
// Which livestream is THIS broadcast's? Whatever was already under "Live Now" when the broadcast began is not (a livestream that was never ended, or one X still
// lists just after it ended), so those ids are remembered first and only a NEW one is taken. The list is only trusted once it has been drawn (its column
// headings are there, or it says there are no livestreams) and two looks in a row agree. It only has a new row once the person has pressed Go Live in Live Studio,
// so it is looked at again and again, with the page reloaded now and then less and less often. It gives up after about a quarter of an hour, and the copied link
// of the go-live window stays as the other way in.
Item {
    id: root
    objectName: "xLiveFinder"
    width: 2; height: 2; clip: true; z: -1
    readonly property bool wanted: Prefs.xSignedIn && Engine.streamRequested && Engine.streamState === "live" && Chat.xId === ""
    property int tries: 0           // looks in this take: kept across a reconnect, so a shaky line does not make it start over for ever
    property bool gaveUp: false
    property var onAirBefore: null     // the ids that were already on air when the broadcast began (null until the list has been read twice, the same)
    property string lastSeen: ""
    Connections {
        target: Engine
        function onStateChanged() { if (Engine.state === "idle") { root.tries = 0; root.gaveUp = false; root.onAirBefore = null; root.lastSeen = "" } }
    }
    // https only, a plain host name (no user name before it, no other port), and it is x.com or twitter.com or a subdomain of one of them.
    function onXHost(target) {
        const m = /^https:\/\/([A-Za-z0-9.-]+)(?::443)?(?:[\/?#]|$)/.exec(target)
        return m !== null && ["x.com", "twitter.com"].some(function (d) { return m[1] === d || m[1].endsWith("." + d) })
    }
    readonly property string findScript: [
        "(function () {",
        "  var heads = /^(livestreams|live now|upcoming|scheduled|past|past livestreams|previous|previous livestreams|ended|recent|completed|earlier|archive|archived)$/, section = '', ids = [];",
        "  var walker = document.createTreeWalker(document.body, NodeFilter.SHOW_ELEMENT | NodeFilter.SHOW_TEXT), node;",
        "  while ((node = walker.nextNode())) {",
        "    if (node.nodeType === 3) { var t = node.textContent.replace(/\\s+/g, ' ').trim().toLowerCase(); if (heads.test(t)) section = t; }",
        "    else if (node.tagName === 'A' && section === 'live now') { var m = /^\\/live\\/([A-Za-z0-9]{10,16})(?:[?#].*)?$/.exec(node.getAttribute('href') || ''); if (m && ids.indexOf(m[1]) < 0) ids.push(m[1]); }",
        "  }",
        "  var text = document.body.innerText.replace(/\\s+/g, ' ');",
        "  return JSON.stringify({ ready: /visibility/i.test(text) || /no livestreams yet/i.test(text), ids: ids });",
        "})()"
    ].join("\n")
    Loader {
        active: root.wanted && !root.gaveUp
        sourceComponent: Component {
            WebEngineView {
                id: web
                objectName: "xLiveFinderPage"
                width: 1400; height: 900
                enabled: false
                audioMuted: true         // a page of X may play sound: it must never reach the desktop sound that goes on air
                profile: xProfile
                url: xStudioUrl
                backgroundColor: "#000000"
                settings.javascriptCanAccessClipboard: false
                settings.javascriptCanOpenWindows: false
                settings.screenCaptureEnabled: false
                settings.fullScreenSupportEnabled: false
                settings.pluginsEnabled: false
                settings.focusOnNavigationEnabled: false
                onWebAuthUxRequested: function (request) { request.cancel() }
                onLoadingChanged: function (info) { if (info.status === WebEngineView.LoadSucceededStatus) look.restart() }
                onNavigationRequested: function (request) {
                    const target = request.url.toString()
                    const testPage = xStudioUrl.indexOf("file://") === 0 && target.indexOf("file://") === 0   // a stand-in page, only when a test points the address at a file
                    if (!(root.onXHost(target) || testPage || target === "about:blank")) request.action = WebEngineNavigationRequest.IgnoreRequest
                }
                Timer {
                    id: look
                    interval: 2500; repeat: true; running: !web.loading
                    onTriggered: {
                        root.tries++
                        if (root.tries > 400) { root.gaveUp = true; return }   // about a quarter of an hour: the person is not going to press Go Live
                        const reloadEvery = root.tries < 20 ? 4 : root.tries < 60 ? 12 : 24   // every 10 s, then every 30 s, then every minute
                        web.runJavaScript(root.findScript, WebEngineScript.ApplicationWorld, function (result) {
                            let info = {}
                            try { info = JSON.parse(result) } catch (e) { info = {} }
                            if (info.ready !== true) { if (root.tries % reloadEvery === 0) web.reload(); return }   // the list has not been drawn yet
                            const key = JSON.stringify(info.ids)
                            if (root.onAirBefore === null) {                  // what is on air now is not this broadcast's: remember it, once the list has stopped changing
                                if (key === root.lastSeen) { root.onAirBefore = info.ids; Chat.setStaleIds(info.ids) }
                                else root.lastSeen = key
                                return
                            }
                            const fresh = info.ids.filter(function (id) { return root.onAirBefore.indexOf(id) < 0 })
                            if (fresh.length > 0) Chat.attachX("https://studio.x.com/live/" + fresh[0])
                            else if (root.tries % reloadEvery === 0) web.reload()
                        })
                    }
                }
            }
        }
    }
}
