import QtQuick
import QtWebEngine
import MobsCore

// The chat of the person's own livestream, the way X shows it to them. Read-only: the box for typing is cut off, because the person answers out loud.
// X's Live Studio page is a desktop page. Below about 900 px wide it turns into a phone layout where the chat card sits far down the page, so MOBS never
// gives the page a small window. The page lives in a big hidden one (pageW x pageH: the desktop layout, always) and only the "Live Chat" card's part of it
// is shown, scaled to the width of the dock. The card is found by what it SAYS (its heading), never by a class name that X can change any day, and the
// page is never changed: MOBS reads where the card is, and nothing else, from it.
// The address is Live Studio itself (studio.x.com/live/<id>). x.com/i/live-studio/<id> holds the same page inside an iframe, which a script on the outer page cannot see into.
Item {
    id: root
    objectName: "xChatView"
    clip: true
    property string livestreamId: Chat.xId
    property string cropState: "waiting"      // waiting, shown, signedout (X wants a sign-in) or missing (the page has no chat card)
    property int tries: 0
    property int misses: 0
    // The shown part of the card, in the page's own pixels, and whether the box for typing was cut off its bottom.
    property real regionX: 0
    property real regionY: 0
    property real regionW: 0
    property real regionH: 0
    property bool inputCut: false
    readonly property real pageW: 1400
    readonly property real pageH: 1200
    readonly property real fit: regionW > 0 && width > 0 ? Math.max(0.25, Math.min(5, width / regionW)) : 1
    onLivestreamIdChanged: { cropState = "waiting"; tries = 0; misses = 0; regionW = 0; regionH = 0 }
    readonly property string findScript: [
        "(function () {",
        "  var rectOf = function (e) { var r = e.getBoundingClientRect(); return { x: r.left, y: r.top, w: r.width, h: r.height }; };",
        "  var heading = null, walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);",
        "  while (walker.nextNode()) { if (walker.currentNode.textContent.replace(/\\s+/g, ' ').trim().toLowerCase() === 'live chat') { heading = walker.currentNode.parentElement; break; } }",
        "  if (!heading) return JSON.stringify({ state: 'none', login: location.href.indexOf('/login') >= 0 || location.href.indexOf('/i/flow/') >= 0 || /sign in|log in|see what.s happening/i.test(document.body.innerText.slice(0, 800)) });",
        "  var card = heading;",
        "  while (card.parentElement && card.parentElement !== document.body) { var r0 = rectOf(card); if (r0.h >= 180 && r0.w >= 200) break; card = card.parentElement; }",
        "  var cr = rectOf(card);",
        "  if (cr.y < 0 || cr.y + cr.h > window.innerHeight) { card.scrollIntoView({ block: 'start' }); cr = rectOf(card); }",
        "  var bottom = cr.y + cr.h, boxes = card.querySelectorAll('input,textarea,[contenteditable=\"true\"],[role=\"textbox\"]');",
        "  for (var k = 0; k < boxes.length; k++) { var row = boxes[k]; while (row.parentElement && row.parentElement !== card && rectOf(row.parentElement).h < 100) row = row.parentElement; var rr = rectOf(row); if (rr.y > cr.y + 60 && rr.y < bottom) bottom = rr.y; }",
        "  return JSON.stringify({ state: 'ok', x: cr.x, y: cr.y, w: cr.w, h: bottom - cr.y, input: bottom < cr.y + cr.h });",
        "})()"
    ].join("\n")

    WebEngineView {
        id: web
        enabled: false                          // for looking at, not for clicking: a click or a drag goes to the dock behind it
        width: root.pageW * root.fit            // with the zoom below this keeps the page 1400 CSS pixels wide, whatever size the dock is
        height: root.pageH * root.fit
        x: -root.regionX * root.fit
        y: root.regionH * root.fit >= root.height ? root.height - (root.regionY + root.regionH) * root.fit : -root.regionY * root.fit   // a short dock shows the NEWEST messages, at the bottom of the card
        zoomFactor: root.fit
        profile: xProfile
        url: root.livestreamId !== "" ? xChatUrlTemplate.replace("%1", root.livestreamId) : "about:blank"
        backgroundColor: "#000000"
        settings.javascriptCanAccessClipboard: false
        settings.javascriptCanOpenWindows: false
        settings.screenCaptureEnabled: false
        settings.fullScreenSupportEnabled: false
        settings.pluginsEnabled: false
        settings.focusOnNavigationEnabled: false
        onLoadingChanged: function (info) { if (info.status === WebEngineView.LoadSucceededStatus) { root.tries = 0; root.misses = 0; look.restart() } }
        onNavigationRequested: function (request) {   // the card is for reading: nothing in it takes the view to another site
            const target = request.url.toString()
            const host = /^https:\/\/([^\/?#:]+)/.exec(target)
            const onX = host && ["x.com", "twitter.com"].some(function (d) { return host[1] === d || host[1].endsWith("." + d) })
            const testPage = xChatUrlTemplate.indexOf("file://") === 0 && target.indexOf("file://") === 0   // a stand-in page, only when a test points the template at a file
            if (!(onX || testPage || target === "about:blank")) request.action = WebEngineNavigationRequest.IgnoreRequest
        }
    }
    Timer {   // X redraws its page now and then: the card is looked for again, and again
        id: look
        interval: 1500; repeat: true; running: root.livestreamId !== "" && !web.loading
        onTriggered: {
            root.tries++
            web.runJavaScript(root.findScript, function (result) {
                let info = {}
                try { info = JSON.parse(result) } catch (e) { info = {} }
                if (info.state === "ok") {
                    root.misses = 0
                    // The cover comes off only when two readings in a row agree: the first one moves and zooms the view, and the page needs a moment to be drawn at
                    // that zoom. Without the wait the whole page flashes on screen (and on air) before the card.
                    const moved = Math.abs(info.x - root.regionX) > 1 || Math.abs(info.y - root.regionY) > 1 || Math.abs(info.w - root.regionW) > 1 || Math.abs(info.h - root.regionH) > 1
                    root.regionX = info.x; root.regionY = info.y; root.regionW = info.w; root.regionH = info.h
                    root.inputCut = info.input === true
                    if (!moved) root.cropState = "shown"
                } else {
                    root.misses++
                    if (root.misses >= (root.cropState === "shown" ? 8 : xChatTries)) root.cropState = info.login === true ? "signedout" : "missing"
                }
            })
        }
    }
    Rectangle {
        objectName: "xChatNotice"
        anchors.fill: parent
        visible: root.cropState !== "shown"
        color: "#000000"
        Txt {
            anchors.centerIn: parent
            width: parent.width - 24; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            font.pixelSize: 13; color: Omarchy.tertiaryText
            text: root.cropState === "signedout" ? "Sign in to X to see the chat here. The setup guide has a chat page for it."
                : root.cropState === "missing" ? "The chat is not on X's page yet. It appears once you are live."
                : "loading your chat..."
        }
    }
}
