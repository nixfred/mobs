import QtQuick
import MobsCore

// Goes live in X Live Studio FOR the person, in MOBS's own browser, so nobody has to find a button in X (Fred, 2026-10-13: "it was confusing what to press and the GO LIVE didn't activate
// on the x.com"). What stopped him was the Title: Live Studio keeps Go Live grey until the form has one, and the old steps never said so. This does what a careful person would do,
// one thing per look at the page: open New Livestream, type the title, check who can watch, wait until X itself says the video has arrived (Go Live un-greys: X looks every 20 seconds,
// so that takes a little while), press Go Live, and read the livestream's id from the address X moves to. If anything is not as expected it says so in X's own words and the
// person can finish by hand in the same box.
Item {
    id: root
    objectName: "livePilot"
    // in
    property bool running: false                       // the go-live window is up, the broadcast is on its way, and there is no livestream yet
    readonly property string wantedTitle: Prefs.liveTitleForX
    readonly property string wantedVisibility: Prefs.livePublic ? "public" : "private"
    readonly property string wantedSource: Prefs.sourceName
    // out
    property string phase: "idle"                      // idle, loading, signin, asking, opening, filling, waiting, pressing, done, failed
    property string note: ""                           // one plain sentence for the person
    property string reason: ""                         // X's own words while Go Live is not available yet
    property string failure: ""                        // why it gave up
    property string livestream: ""                     // the id of the livestream, once it exists
    property int progress: 0                           // 0 nothing yet, 1 the form is open, 2 it is filled in, 3 X sees the video, 4 live
    readonly property bool working: running && phase !== "idle" && phase !== "done" && phase !== "failed"
    readonly property bool needsPerson: phase === "signin" || phase === "asking" || phase === "failed"
    property var view: null                            // the XStudioView it works in (the go-live window makes it only while the pilot runs: a browser costs memory)
    signal live(string id)

    // the clock of this run, and what has been tried
    property double phaseSince: 0
    property double waitingSince: 0                    // since Go Live was last seen grey
    property double pressedAt: 0
    property double reloadedAt: 0                      // when the page was last loaded again (the give-up clock is the phase's, and keeps running)
    property int pressTries: 0
    property int openTries: 0
    property int titleTries: 0
    property int visibilityTries: 0
    property int sourceTries: 0
    property int detailsTries: 0
    property int reloads: 0
    property bool visibilityDone: false
    property string lastLine: ""
    property string seenTitle: ""                      // what the form showed as its title the last time the page was looked at (for the checks)
    property string seenVisibility: ""
    property string seenSource: ""

    function note_(text) { if (text !== lastLine) { lastLine = text; App.note("pilot", text) } }
    function enter(next, text) {
        if (next !== phase) { phase = next; phaseSince = Date.now(); note_(next + ": " + text) }
        if (text !== note) note = text
    }
    function fail(why) {
        failure = why
        reason = ""
        enter("failed", why)
    }
    function succeed(id) {
        if (phase === "done") return
        livestream = id
        progress = 4
        enter("done", "You're live.")
        note_("livestream created: its address carries the id (not logged)")
        live(id)
    }
    function reset() {
        phase = "idle"; note = ""; reason = ""; failure = ""; livestream = ""; progress = 0
        phaseSince = 0; waitingSince = 0; pressedAt = 0; reloadedAt = 0; lastLine = ""
        pressTries = openTries = titleTries = visibilityTries = sourceTries = detailsTries = reloads = 0
        visibilityDone = false
    }
    function onStudio(st) { return view.testPage || st.host === "studio.x.com" }

    // Who can watch could not be read or set. X starts a livestream as Public, so a person who wants Public can go on; a person who asked for PRIVATE must not be published to everyone.
    function giveUpOnVisibility() {
        if (wantedVisibility === "private") { fail("MOBS could not set the livestream to private, so it did not press Go Live. Set it to private in the box, then press Go Live."); return false }
        visibilityDone = true; note_("who can watch could not be checked: left as X had it")
        return true
    }

    // One look at the page, and at most one job given to it.
    function advance(st) {
        const now = Date.now()
        if (st.missing || st.error) { view.inject(); if (st.error) note_("the page helper reported a problem: " + st.error.slice(0, 120)); return }   // the page is still loading, or it is not Live Studio
        if (st.dialog) { seenTitle = st.title === null || st.title === undefined ? "" : st.title; if (st.visibility !== "") seenVisibility = st.visibility; seenSource = st.source || "" }
        if (typeof st.live === "string" && st.live !== "" && (pressedAt > 0 || phase === "failed")) { succeed(st.live); return }   // the livestream exists: X moved to its page
        if (phase === "failed") return                                                              // from here on only watching for the person to finish by hand
        if (Engine.streamState === "failed" || Engine.streamState === "ended") { fail("Your video is not reaching X any more, so there is nothing to go live with."); return }
        if (st.signedOut) { reason = ""; enter("signin", "Sign in to X in the box. MOBS carries on by itself afterwards."); return }
        if (phase === "signin") { enter("loading", "Signed in. Opening Live Studio..."); if (!onStudio(st)) view.url = xStudioUrl; return }
        if (!onStudio(st)) { if (now - phaseSince > 4000) { view.url = xStudioUrl; phaseSince = now } return }   // X sent the person to its home page after the password
        if (!Prefs.xSignedIn && (st.newButton !== "" || st.dialog)) Prefs.setXSignedIn(true)   // Live Studio drew its own page: the person is signed in (the chat card and the livestream finder rely on it)
        if (!st.dialog && st.other !== "") { enter("asking", "X is asking something: " + st.other.slice(0, 140) + " Answer it in the box."); return }
        if (phase === "asking") enter("opening", "Opening the livestream form...")

        if (!st.dialog) {
            if (pressedAt > 0) { if (now - pressedAt > 25000) fail("X did not move to your livestream after Go Live."); return }   // pressed: the form closes and the address changes
            if (st.newButton === "on") {
                if (openTries >= 8) { fail("MOBS could not open the livestream form."); return }
                openTries++
                enter("opening", "Opening the livestream form...")
                view.ask("openNew", undefined, function () {})
                return
            }
            enter("loading", "Opening Live Studio...")
            if (now - Math.max(phaseSince, reloadedAt) > 30000 && reloads < 3) { reloads++; reloadedAt = now; note_("Live Studio did not show its New Livestream button: loading it again"); view.reload() }
            else if (now - phaseSince > pilotPatienceMs) fail("Live Studio did not open for this account. It needs X Premium.")
            return
        }

        // the form is open
        if (progress < 1) progress = 1
        if (pressedAt > 0) {   // Go Live was pressed and the form is still there: a press can be lost while X is busy
            if (st.toast !== "" && /error|couldn.?t|could not|failed|try again/i.test(st.toast)) { fail("X said: " + st.toast); return }
            if (now - pressedAt > 12000 && st.goLive === "on" && pressTries < 3) { pressTries++; pressedAt = now; note_("Go Live was still there after 12 s: pressing it again"); view.ask("goLive", undefined, function () {}) }
            else if (now - pressedAt > 40000) fail("X did not start the livestream.")
            return
        }
        const onVisibilityStep = st.tab === "audience"
        if (!visibilityDone && st.tab !== "details" && !onVisibilityStep) {
            if (++detailsTries > 8) { fail("MOBS could not find the Details part of the form."); return }
            view.ask("tab", "Details", function () {}); return
        }
        if (!onVisibilityStep && st.title !== null && st.title !== root.wantedTitle) {   // the title X needs before it lets anyone go live
            if (++titleTries > 6) { fail("MOBS could not type the title into the form."); return }
            enter("filling", "Filling in your livestream: the title and who can watch.")
            view.ask("setTitle", root.wantedTitle, function () {})
            note_("typing the title (" + root.wantedTitle.length + " characters)")
            return
        }
        if (!visibilityDone) {   // X's own default is Public, but it is checked rather than assumed: a Private livestream is what showed "0 views" on 2026-10-12
            enter("filling", "Filling in your livestream: the title and who can watch.")
            if (!onVisibilityStep) {
                if (++visibilityTries > 5) { if (!giveUpOnVisibility()) return; return }
                view.ask("tab", "Audience", function () {})
                return
            }
            if (st.visibility === root.wantedVisibility) { visibilityDone = true; note_("who can watch: " + st.visibility); view.ask("tab", "Details", function () {}); return }
            if (++visibilityTries > 5) { if (giveUpOnVisibility()) view.ask("tab", "Details", function () {}); return }
            view.ask("setVisibility", root.wantedVisibility, function () {})
            return
        }
        if (root.wantedSource !== "" && st.source !== "" && st.source.toLowerCase() !== root.wantedSource.toLowerCase() && sourceTries >= 6) {   // never go live on another source than the one MOBS sends to
            fail("The form is on the source " + st.source + ", not " + root.wantedSource + ", so MOBS did not press Go Live. Choose " + root.wantedSource + " in the box, then press Go Live.")
            return
        }
        if (root.wantedSource !== "" && st.source.toLowerCase() !== root.wantedSource.toLowerCase() && sourceTries < 6) {   // the person named the source they use: the form is made to say it
            sourceTries++
            enter("filling", "Filling in your livestream: choosing your source, " + root.wantedSource + ".")
            if (st.options.length === 0) view.ask("openSources", undefined, function () {})
            else view.ask("chooseSource", root.wantedSource, function () {})
            return
        }
        if (progress < 2) progress = 2
        if (st.goLive === "on") {
            reason = ""
            progress = 3
            pressTries = 1
            pressedAt = now
            enter("pressing", "X sees your video. Pressing Go Live...")
            view.ask("goLive", undefined, function () {})
            return
        }
        // Go Live is grey: X says why, in its own words
        reason = st.reason
        if (waitingSince === 0) waitingSince = now
        const waited = now - waitingSince
        const sourceOff = /source must be on/i.test(st.reason)
        enter("waiting", sourceOff ? "Waiting for X to see your video. X looks every 20 seconds, so this can take up to a minute."
                                   : st.reason !== "" ? "X says: " + st.reason : "Waiting for X to let you go live...")
        if (sourceOff && waited > 40000 && reloads < 2) {   // the status X polls can be stale: a fresh page asks X again
            reloads++; waitingSince = now
            note_("X still shows the source as off after 40 s: loading Live Studio again")
            view.ask("closeDialog", undefined, function () { view.reload() })
            visibilityDone = false; visibilityTries = 0; titleTries = 0; openTries = 0
            return
        }
        if (waited > pilotPatienceMs) fail(sourceOff ? "X still does not see your video, although MOBS is sending it." : "X would not let the livestream start" + (st.reason !== "" ? ": " + st.reason : "."))
    }

    property bool asking: false
    function step() {
        if (asking || !view) return
        asking = true
        if (!view.ask("look", undefined, function (st) { asking = false; if (root.running) root.advance(st) })) asking = false
    }
    onRunningChanged: {
        if (running) { reset(); enter("loading", "Opening Live Studio...") }
        else if (phase !== "idle" && phase !== "done" && phase !== "failed") fail("Your video is not reaching X any more, so there is nothing to go live with.")   // the hand steps show, the box is the person's
    }
    Timer { interval: pilotTickMs; repeat: true; running: root.running && root.phase !== "done"; onTriggered: root.step() }
}
