import QtQuick
import QtWebEngine
import MobsCore

// The chat of the person's own livestream, the way X shows it to them. MOBS loads the livestream's Live Studio page in its own browser (where the person
// signed in once) and shows ONLY the "Live Chat" card of it, so the card under the webcam holds the chat and nothing else. It is read-only on purpose:
// the box for typing is hidden too, because the person answers out loud.
// The page is X's, not MOBS's, so the card is found by what it SAYS (its heading), never by a class name that X can change any day. If it is not found
// after a while, the whole page is shown, small, so the chat is still there to read.
Item {
    id: root
    objectName: "xChatView"
    property string livestreamId: Chat.xId
    property string cropState: "waiting"      // waiting, cropped or whole
    property int hiddenCount: 0               // how many other parts of the page were hidden
    property int tries: 0
    readonly property string cropScript: [
        "(function () {",
        "  var heading = null, els = document.querySelectorAll('div,span,h1,h2,h3,p');",
        "  for (var i = 0; i < els.length; i++) { var e = els[i]; if (e.children.length === 0 && e.textContent.trim().toLowerCase() === 'live chat') { heading = e; break; } }",
        "  if (!heading) return JSON.stringify({ state: 'none' });",
        "  var card = heading;",
        "  while (card.parentElement && card.parentElement !== document.body) { var r = card.getBoundingClientRect(); if (r.height >= 180 && r.width >= 200) break; card = card.parentElement; }",
        "  var hidden = 0, el = card;",
        "  while (el && el !== document.documentElement) {",
        "    var parent = el.parentElement; if (!parent) break;",
        "    for (var j = 0; j < parent.children.length; j++) { var sib = parent.children[j]; if (sib !== el && sib.tagName !== 'SCRIPT' && sib.tagName !== 'STYLE' && sib.style.display !== 'none') { sib.style.setProperty('display', 'none', 'important'); hidden++; } }",
        "    el = parent;",
        "  }",
        "  var s = card.style; s.setProperty('position', 'fixed', 'important'); s.setProperty('inset', '0', 'important'); s.setProperty('width', '100vw', 'important'); s.setProperty('height', '100vh', 'important'); s.setProperty('margin', '0', 'important'); s.setProperty('z-index', '2147483647', 'important'); s.setProperty('background', '#000', 'important');",
        "  var boxes = card.querySelectorAll('input,textarea,[contenteditable=\"true\"],[role=\"textbox\"]');",
        "  for (var k = 0; k < boxes.length; k++) { var row = boxes[k]; while (row.parentElement && row.parentElement !== card && row.parentElement.getBoundingClientRect().height < 100) row = row.parentElement; row.style.setProperty('display', 'none', 'important'); }",
        "  document.documentElement.style.setProperty('overflow', 'hidden', 'important'); document.body.style.setProperty('overflow', 'hidden', 'important');",
        "  return JSON.stringify({ state: 'ok', hidden: hidden });",
        "})()"
    ].join("\n")

    WebEngineView {
        id: web
        anchors.fill: parent
        profile: xProfile
        url: root.livestreamId !== "" ? xChatUrlTemplate.replace("%1", root.livestreamId) : "about:blank"
        backgroundColor: "#000000"
        settings.javascriptCanAccessClipboard: false
        settings.javascriptCanOpenWindows: false
        settings.screenCaptureEnabled: false
        settings.fullScreenSupportEnabled: false
        settings.pluginsEnabled: false
        settings.focusOnNavigationEnabled: false
        onLoadingChanged: function (info) { if (info.status === WebEngineView.LoadSucceededStatus) { root.tries = 0; crop.restart() } }
        onNavigationRequested: function (request) {   // the chat card is for reading: nothing in it takes the card to another site
            const target = request.url.toString()
            const testFile = xChatUrlTemplate.indexOf("file://") === 0 && target.indexOf("file://") === 0   // a stand-in page, only when a test points the template at a file
            if (!(target.indexOf("https://x.com/") === 0 || testFile || target === "about:blank")) request.action = WebEngineNavigationRequest.IgnoreRequest
        }
    }
    Timer {   // X redraws its page now and then: the crop is simply done again, and again finds nothing left to hide
        id: crop
        interval: 1500; repeat: true; running: root.livestreamId !== "" && !web.loading
        onTriggered: {
            root.tries++
            web.runJavaScript(root.cropScript, function (result) {
                let info = {}
                try { info = JSON.parse(result) } catch (e) { info = {} }
                if (info.state === "ok") { root.cropState = "cropped"; root.hiddenCount = Math.max(root.hiddenCount, info.hidden || 0) }
                else if (root.tries >= 8 && root.cropState !== "cropped") { root.cropState = "whole"; web.zoomFactor = 0.6 }
            })
        }
    }
    Rectangle {
        anchors.fill: parent
        visible: root.cropState === "waiting"
        color: "#000000"
        Txt { anchors.centerIn: parent; text: "loading your chat…"; font.pixelSize: 13; color: Omarchy.tertiaryText }
    }
}
