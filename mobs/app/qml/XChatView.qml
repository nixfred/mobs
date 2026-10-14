import QtQuick
import QtWebEngine
import MobsCore

// The chat of the person's own livestream, the way X shows it to them. Read-only: the box for typing is cut off, because the person answers out loud.
// X's Live Studio page is a desktop page: below about 900 px wide it turns into a phone layout. MOBS gives the page a big hidden window (pageW x pageH: 960 wide, X's stacked layout,
// see below) and shows only the card's MESSAGES, scaled to the width of the dock and ending at the newest one (X's header of the card, its gap under the header, the three dots of
// every message and a message the list has scrolled half out of sight are not shown). The card is found by what it SAYS (its heading), never by a class name that X can change any
// day. The page is read, not driven, with three exceptions that are all MOBS's own private view of it: one style rule hides the three dots of every message; the numbers of people
// watching are read from its words ("3 viewers watching now"); and the kill switch presses End livestream and confirms it (the End sequence below), which is the only click this
// view ever makes. The address is Live Studio itself (studio.x.com/live/<id>). x.com/i/live-studio/<id> holds the same page inside an iframe, which a script on the outer page cannot see into.
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
    property int freshBeats: 0                // how many fresh frames the timer below has asked Chromium for since this livestream was attached (the tests read it)
    property int moves: 0                     // readings in a row where the card was somewhere else than in the one before
    property int messages: 0                  // how many messages the card shows (0: nobody has written yet, and the dock says so instead of showing an empty card)
    // The page is 960 CSS pixels wide, which is X's stacked (tablet) layout: the chat card is then about 660 pixels wide and sits below the video, instead of a narrow column of 363 pixels
    // at the side (the desktop layout, from about 1280). Scaled to the width of the dock a wide card gives text about as big as the dock's own (a narrow one blew the text up to 2.8 times).
    // The card is found by what it says and scrolled into view, so where it sits does not matter. The height has room for the whole card below the video.
    readonly property real pageW: 960
    readonly property real pageH: 1500
    readonly property real fit: regionW > 0 && width > 0 ? Math.max(0.25, Math.min(5, width / regionW)) : 1
    onLivestreamIdChanged: { cropState = "waiting"; tries = 0; misses = 0; moves = 0; blackTries = 0; regionW = 0; regionH = 0; freshBeats = 0; messages = 0; endTouched = false; endGone = 0 }
    // https only, a plain host name (no user name before it, no other port), and it is x.com or twitter.com or a subdomain of one of them.
    function onXHost(target) {
        const m = /^https:\/\/([A-Za-z0-9.-]+)(?::443)?(?:[\/?#]|$)/.exec(target)
        return m !== null && ["x.com", "twitter.com"].some(function (d) { return m[1] === d || m[1].endsWith("." + d) })
    }
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
        "  // the header is the heading and its two buttons (pop out, who may chat), found by their LABELS: any other button near the top is a message's (the first message of a short chat sits right under the header)",
        "  var hr = rectOf(heading), headBottom = hr.y + hr.h, hb = card.querySelectorAll('button[aria-label=\"Pop out chat\"],button[aria-label^=\"Chat audience\"]'), j, bb;",
        "  for (j = 0; j < hb.length; j++) { bb = rectOf(hb[j]); if (bb.h > 0 && bb.y + bb.h > headBottom && bb.y + bb.h < cr.y + 120) headBottom = bb.y + bb.h; }",
        "  // the list is the box that scrolls the messages: what lies fully inside it is shown, a message the list has scrolled half out of sight is not",
        "  var listTop = headBottom + 4, listBottom = bottom, tw0 = document.createTreeWalker(card, NodeFilter.SHOW_TEXT), t0, e0, sr, foundList = false;",
        "  while ((t0 = tw0.nextNode())) {",
        "    if (!t0.textContent.trim() || heading.contains(t0)) continue;",
        "    for (e0 = t0.parentElement; e0 && e0 !== card; e0 = e0.parentElement) { if (/(auto|scroll)/.test(getComputedStyle(e0).overflowY)) { sr = rectOf(e0); if (sr.h >= 80) {",
        "      // the dock is for the NEWEST messages: a list that is not at its end is taken there (X does it on its own page while a person is looking; nobody scrolls this one)",
        "      if (e0.scrollTop + e0.clientHeight < e0.scrollHeight - 2) { e0.scrollTop = e0.scrollHeight; sr = rectOf(e0); }",
        "      listTop = Math.max(listTop, sr.y); listBottom = Math.min(listBottom, sr.y + sr.h); foundList = true; break; } } }",
        "    if (foundList) break;",
        "  }",
        "  var first = -1, last = listTop, rows = 0, texts = document.createTreeWalker(card, NodeFilter.SHOW_TEXT), tn;",
        "  while ((tn = texts.nextNode())) { if (!tn.textContent.trim() || heading.contains(tn)) continue; var rg = document.createRange(); rg.selectNodeContents(tn); var tb = rg.getBoundingClientRect(); if (tb.height > 0 && tb.top >= listTop - 1 && tb.bottom <= listBottom + 1) { if (first < 0 || tb.top < first) first = tb.top; if (tb.bottom > last) last = tb.bottom; } }",
        "  var pics = card.querySelectorAll('img,canvas,video'), pb, qq;",
        "  for (qq = 0; qq < pics.length; qq++) { pb = pics[qq].getBoundingClientRect(); if (pb.height > 0 && pb.width > 0 && pb.top >= listTop - 1 && pb.bottom <= listBottom + 1) { rows++; if (first < 0 || pb.top < first) first = pb.top; if (pb.bottom > last) last = pb.bottom; } }",
        "  var mores = card.querySelectorAll('button[aria-label=\"More actions\"]'), mm, mc = 0;",
        "  for (qq = 0; qq < mores.length; qq++) { mm = rectOf(mores[qq]); if (mm.h > 0 && mm.y >= listTop - 1 && mm.y + mm.h <= listBottom + 1) mc++; }",
        "  var input = bottom < cr.y + cr.h;",
        "  if (first < 0) return JSON.stringify({ state: 'ok', x: cr.x, y: cr.y, w: cr.w, h: Math.max(40, hr.y + hr.h + 14 - cr.y), input: input, rows: 0 });",
        "  var top = Math.max(listTop, first - 8), end = Math.min(listBottom - 1, last + 12);   // a pixel inside the card, so its edge never bleeds into what is behind it",
        "  return JSON.stringify({ state: 'ok', x: cr.x, y: top, w: cr.w, h: Math.max(30, end - top), input: input, rows: Math.max(rows, mc) });",
        "})()",
    ].join("\n")
    readonly property string viewersScript: [
        "(function () {",
        "  var all = document.body ? document.body.innerText : '', t = all;",
        "  // the number is read inside the card called Concurrent viewers when that can be found (a person in the chat could type the same words)",
        "  var walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT), card = null;",
        "  while (walker.nextNode()) { if (walker.currentNode.textContent.replace(/\\s+/g, ' ').trim().toLowerCase() === 'concurrent viewers') { card = walker.currentNode.parentElement; break; } }",
        "  while (card && card.parentElement && card.parentElement !== document.body && card.getBoundingClientRect().height < 100) card = card.parentElement;",
        "  if (card) t = card.innerText || all;",
        "  var w = /([0-9][0-9,]*)\\s+viewers?\\s+watching\\s+now/i.exec(t), v = /([0-9][0-9.,]*)\\s?([KkMm]?)\\s+views?\\b/.exec(all);",
        "  var views = -1; if (v) { views = parseFloat(v[1].replace(/,/g, '')); if (/k/i.test(v[2])) views *= 1000; else if (/m/i.test(v[2])) views *= 1000000; views = Math.round(views); }",
        "  return JSON.stringify({ w: w ? parseInt(w[1].replace(/,/g, ''), 10) : /no viewer data/i.test(t) ? 0 : -1, v: views });",
        "})()",
    ].join("\n")
    readonly property string cleanScript: [
        "(function () {",
        "  if (document.getElementById('mobs-chat-clean')) return 'there';",
        "  var st = document.createElement('style'); st.id = 'mobs-chat-clean';",
        "  st.textContent = 'button[aria-label=\"More actions\"] { visibility: hidden !important }';   // the three dots on every message: the dock is for reading",
        "  (document.head || document.documentElement).appendChild(st); return 'added';",
        "})()",
    ].join("\n")
    readonly property string endScript: [
        "(function () {",
        "  var vis = function (e) { var r = e.getBoundingClientRect(); return r.width > 0 && r.height > 0; };",
        "  var dialogs = [].slice.call(document.querySelectorAll('[role=dialog],[role=alertdialog]')).filter(vis);",
        "  var d = dialogs.filter(function (x) { return /end livestream\\?/i.test(x.innerText || ''); })[0];",
        "  if (d) { var c = [].slice.call(d.querySelectorAll('button')).filter(function (b) { return (b.innerText || '').trim() === 'End livestream'; })[0]; if (c) { c.click(); return 'confirmed'; } return 'dialog'; }",
        "  var e = [].slice.call(document.querySelectorAll('button')).filter(function (b) { return b.getAttribute('aria-label') === 'End livestream'; })[0];",
        "  if (e) { e.click(); return 'asked'; }",
        "  if (/set replay start here/i.test(document.body ? document.body.innerText : '')) return 'ended';",
        "  return 'none';",
        "})()",
    ].join("\n")


    Item {   // the part of the card that is shown: a short card is cut here (nothing of the page below it may show), a long one fills the dock
        id: shown
        x: 1                                    // a pixel is cut off each side of the card (its edge lands between two pixels, and the page behind it would bleed in)
        width: root.width - 2
        height: root.regionW > 0 ? Math.min(root.height, root.regionH * root.fit) : root.height
        clip: true
    WebEngineView {
        id: web
        enabled: false                          // for looking at, not for clicking: a click or a drag goes to the dock behind it
        audioMuted: true                        // a livestream page plays sound: it must never reach the desktop sound that goes on air
        width: root.pageW * root.fit            // with the zoom below this keeps the page pageW CSS pixels wide, whatever size the dock is
        height: root.pageH * root.fit
        x: -root.regionX * root.fit - 1
        y: root.regionH * root.fit >= shown.height ? shown.height - (root.regionY + root.regionH) * root.fit : -root.regionY * root.fit   // a short dock shows the NEWEST messages, at the bottom of the card
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
        onWebAuthUxRequested: function (request) { request.cancel() }   // the chat card never signs in: a passkey request is refused at once, never left waiting
        onLoadingChanged: function (info) { if (info.status === WebEngineView.LoadSucceededStatus) { root.tries = 0; root.misses = 0; look.restart() } }
        onNavigationRequested: function (request) {   // the card is for reading: nothing in it takes the view to another site
            const target = request.url.toString()
            const onX = root.onXHost(target)
            const testPage = xChatUrlTemplate.indexOf("file://") === 0 && target.indexOf("file://") === 0   // a stand-in page, only when a test points the template at a file
            if (!(onX || testPage || target === "about:blank")) request.action = WebEngineNavigationRequest.IgnoreRequest
        }
    }
    }
    Timer {   // X redraws its page now and then: the card is looked for again, and again
        id: look
        interval: 1500; repeat: true; running: root.livestreamId !== "" && !web.loading
        onTriggered: {
            root.tries++
            web.runJavaScript(root.cleanScript, WebEngineScript.ApplicationWorld)   // hides the three dots of every message (once per page)
            web.runJavaScript(root.findScript, WebEngineScript.ApplicationWorld, function (result) {
                let info = {}
                try { info = JSON.parse(result) } catch (e) { info = {} }
                if (info.state === "ok") {
                    root.misses = 0
                    // The cover comes off only when two readings in a row agree: the first one moves and zooms the view, and the page needs a moment to be drawn at
                    // that zoom. Without the wait the whole page flashes on screen (and on air) before the card.
                    const moved = Math.abs(info.x - root.regionX) > 1 || Math.abs(info.y - root.regionY) > 1 || Math.abs(info.w - root.regionW) > 1   // the height grows with every message and changes nothing about the zoom
                    root.regionX = info.x; root.regionY = info.y; root.regionW = info.w; root.regionH = info.h
                    root.inputCut = info.input === true
                    root.messages = info.rows || 0
                    // A page whose card never holds still (a banner or a counter that keeps moving it) would keep the cover up for ever: after a number of readings that
                    // all moved, the card is shown where it is. A card a pixel off is better than no chat.
                    if (!moved) { root.moves = 0; root.cropState = "shown" }
                    else if (++root.moves >= 8) { root.moves = 0; root.cropState = "shown"; console.info("chat card: the card kept moving; shown where it is") }
                } else {
                    root.misses++
                    if (root.misses >= (root.cropState === "shown" ? 8 : xChatTries)) root.cropState = info.login === true ? "signedout" : "missing"
                    // A page that failed to load, or that was loaded before the person signed in somewhere else in MOBS, never changes by itself: once the card has SAID so
                    // (sign in, or not there yet) the page is asked for again now and then, so the card recovers when the network is back or the sign-in is done. Never before it
                    // has said so: a reload resets the count of misses, and one that came sooner than the verdict would keep the verdict from ever being given.
                    if ((root.cropState === "signedout" || root.cropState === "missing") && root.misses === Math.max(10, xChatTries + 2)) {
                        console.info("chat card: the page has no Live Chat card (" + root.cropState + "): asking X's page again")
                        web.reload()
                    }
                }
            })
        }
    }
    Timer {   // a page that stays "loading" for ever (a stuck connection) never lets the look timer above run: after a while it is stopped and asked for again
        interval: 45000; repeat: true; running: root.livestreamId !== "" && web.loading
        onTriggered: { console.info("chat card: the page was still loading after 45 s: asking X's page again"); web.stop(); web.reload() }
    }
    // How many are watching, read from the page's own words ("3 viewers watching now", and "76 views" for everyone so far). The dock window shows it beside the webcam, with the trend.
    Timer {
        interval: 5000; repeat: true; triggeredOnStart: true
        running: root.livestreamId !== "" && !web.loading
        onTriggered: web.runJavaScript(root.viewersScript, WebEngineScript.ApplicationWorld, function (result) {
            let info = {}
            try { info = JSON.parse(result) } catch (e) { return }
            Chat.noteViewers(info.w, info.v)
        })
    }
    // Ending the livestream on X: this is the page of THAT livestream, signed in as the person, so it presses End livestream and confirms (its dialog asks "End livestream?"). Once the
    // button is gone and the page shows the replay (or the button was pressed and has been gone for a moment) the livestream is over. The model gives up after 14 seconds.
    property bool endTouched: false           // End livestream was pressed or its dialog confirmed
    property int endGone: 0                   // looks in a row, after that, with nothing left to press
    function endStep() {
        if (!Chat.ending) { endTimer.stop(); return }
        web.runJavaScript(root.endScript, WebEngineScript.ApplicationWorld, function (answer) {
            if (!Chat.ending) return
            if (answer === "asked" || answer === "confirmed") { if (answer === "confirmed" || !root.endTouched) App.note("chat", "end: " + answer); root.endTouched = true; root.endGone = 0 }
            else if (answer === "ended") { endTimer.stop(); Chat.endFinished(true) }
            else if (answer === "none" && root.endTouched && ++root.endGone >= 3) { endTimer.stop(); Chat.endFinished(true) }
        })
    }
    Timer { id: endTimer; interval: 700; repeat: true; onTriggered: root.endStep() }
    Connections {
        target: Chat
        function onEndRequested() { root.endTouched = false; root.endGone = 0; endTimer.restart(); root.endStep() }
        function onEndingChanged() { if (!Chat.ending) endTimer.stop() }
    }
    // On some graphics stacks the dock window is handed a still page's LAST frame as black and keeps it until Chromium paints again (seen on a virtual GPU with
    // Qt's threaded render loop: a quiet chat page stayed black for good, a busy one drew). A real livestream page repaints on its own all the time, a quiet one
    // may not. So while the card is shown, the view's own background changes by one shade, now and then: that asks Chromium for a fresh frame, changes nothing on
    // the page (it is never touched) and nothing the eye can see. Asking Qt to redraw the same frame (update()) did not help: the frame itself is the stale thing.
    Timer {
        objectName: "xChatFresh"
        property bool odd: false
        interval: 500; repeat: true; triggeredOnStart: true; running: root.cropState === "shown"
        onTriggered: { odd = !odd; web.backgroundColor = odd ? "#010101" : "#000000"; root.freshBeats++ }
    }
    // A self-check for the log: a moment after the card is shown, again after ten seconds, then every two minutes, the card's own pixels are looked at and the log says how
    // much of it is lit. Black means the card is drawing nothing on this graphics stack, whatever else the log says. Only that one number is kept, never the picture.
    // (Found with the colour change above switched off: the card drew at first and was black ten seconds later, so the first look is not enough.) A black card is not left
    // black: the page is asked for again, which makes Chromium paint, and the card is looked at again six seconds later, three times at the most.
    property int blackTries: 0
    function lookAtTheCard() {
        shown.grabToImage(function (result) {
            if (Pip.chatCardLooks(result.image)) { root.blackTries = 0; return }
            if (root.cropState !== "shown" || root.blackTries >= 3) return
            root.blackTries++
            console.info("chat card: the card is black: asking X's page again (" + root.blackTries + " of 3)")
            web.reload()
            recheck.restart()
        })
    }
    Timer { id: recheck; interval: 6000; onTriggered: root.lookAtTheCard() }
    onFreshBeatsChanged: if (freshBeats === 2 || freshBeats === 20 || (freshBeats > 0 && freshBeats % 240 === 0)) lookAtTheCard()
    Rectangle {   // a list that is full and scrolling is cut mid-message at the top: it fades into the header instead (X does the same on its own page)
        objectName: "xChatFade"
        visible: root.cropState === "shown" && root.messages > 0 && root.regionW > 0 && root.regionH * root.fit > root.height + 1
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: Math.min(root.height * 0.3, root.width * 0.1)
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.alpha(Omarchy.darkBackground, 0.95) }
            GradientStop { position: 1; color: "transparent" }
        }
    }
    Rectangle {
        objectName: "xChatNotice"
        anchors.fill: parent
        visible: root.cropState !== "shown" || root.messages === 0
        color: root.cropState === "shown" ? Omarchy.darkBackground : "#000000"   // while the page is not ready, or nobody has written yet, it is covered (the page itself keeps drawing underneath: a page nobody draws can stop updating)
        Txt {
            anchors.centerIn: parent
            width: parent.width - 24; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            font.pixelSize: Math.max(12, Math.round(root.width * 0.034)); color: Omarchy.tertiaryText
            text: root.cropState === "signedout" ? "Sign in to X to see the chat here. The setup guide has a chat page for it."
                : root.cropState === "missing" ? "The chat is not on X's page yet. It appears once you are live."
                : root.cropState === "shown" ? "waiting for the first message..."
                : "loading your chat..."
        }
    }
}
