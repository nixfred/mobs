import QtQuick
import QtWebEngine
import MobsCore

// While a broadcast is on air and the person is signed in to X, MOBS finds their livestream BY ITSELF, so nobody has to copy a link. Live Studio's list page
// shows the livestream that is on air under "Live Now", with a plain link to it (/live/<id>). The page is loaded in a browser that is only 2 pixels wide on
// screen and sits behind the picture of the webcam: big enough to count as showing, so X draws it, too small to see. Nothing on the page is changed; MOBS
// reads one link from it, and the chat card then opens that livestream's chat.
// The list only has a "Live Now" row once the person has pressed Go Live in Live Studio, so it is looked at again and again (the page is reloaded now and then).
// It gives up after a while, and the copied link of the go-live window stays as the other way in.
Item {
    id: root
    objectName: "xLiveFinder"
    width: 2; height: 2; clip: true; z: -1
    readonly property bool wanted: Prefs.xSignedIn && Engine.streamRequested && Engine.streamState === "live" && Chat.xId === ""
    property int tries: 0
    property bool gaveUp: false
    onWantedChanged: { tries = 0; gaveUp = false }
    readonly property string findScript: [
        "(function () {",
        "  var heads = /^(livestreams|live now|upcoming|scheduled|past|previous|ended|recent)$/, section = '';",
        "  var walker = document.createTreeWalker(document.body, NodeFilter.SHOW_ELEMENT | NodeFilter.SHOW_TEXT), node;",
        "  while ((node = walker.nextNode())) {",
        "    if (node.nodeType === 3) { var t = node.textContent.replace(/\\s+/g, ' ').trim().toLowerCase(); if (heads.test(t)) section = t; }",
        "    else if (node.tagName === 'A' && section === 'live now') { var m = /^\\/live\\/([A-Za-z0-9]{10,16})(?:[?#].*)?$/.exec(node.getAttribute('href') || ''); if (m) return JSON.stringify({ state: 'live', id: m[1] }); }",
        "  }",
        "  return JSON.stringify({ state: 'none' });",
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
                    const host = /^https:\/\/([^\/?#:]+)/.exec(target)
                    const onX = host && ["x.com", "twitter.com"].some(function (d) { return host[1] === d || host[1].endsWith("." + d) })
                    const testPage = xStudioUrl.indexOf("file://") === 0 && target.indexOf("file://") === 0
                    if (!(onX || testPage || target === "about:blank")) request.action = WebEngineNavigationRequest.IgnoreRequest
                }
                Timer {
                    id: look
                    interval: 2500; repeat: true; running: !web.loading
                    onTriggered: {
                        root.tries++
                        if (root.tries > 400) { root.gaveUp = true; return }   // about a quarter of an hour: the person is not going to press Go Live
                        web.runJavaScript(root.findScript, function (result) {
                            let info = {}
                            try { info = JSON.parse(result) } catch (e) { info = {} }
                            if (info.state === "live") Chat.attachX("https://studio.x.com/live/" + info.id)
                            else if (root.tries % 4 === 0) web.reload()
                        })
                    }
                }
            }
        }
    }
}
