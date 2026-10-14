import QtQuick
import MobsCore

// Finds the person's source on X and takes its stream key and server address FROM X, so nobody copies a key by hand (Fred, 2026-10-13: the manual key steps were where a fresh setup
// went wrong, and a source made in another region than MOBS's built-in address shows "Not connected" for ever). In MOBS's own browser, signed in as the person, it opens the Sources page
// of Live Studio, makes a source called MOBS when there is none (X offers the region it likes: whatever address X then shows is the one MOBS uses), opens the key panel of the source,
// shows the key and reads the key and the address. The key goes straight into MOBS's private key file; it is never put in a log, a note or a property.
Item {
    id: root
    objectName: "sourcePilot"
    // in
    property bool running: false
    property var view: null
    // out
    property string phase: "idle"             // idle, loading, signin, looking, creating, reading, done, failed
    property string note: ""
    property string failure: ""
    property string sourceName: ""
    property bool signedIn: false
    property bool found: false                // a source was found (or made)
    property bool keySaved: false             // its key and address were read and saved
    readonly property bool needsPerson: phase === "signin" || phase === "failed"

    property double phaseSince: 0
    property int emptyLooks: 0                // looks in a row at a drawn Sources page with no source on it
    property int createTries: 0
    property int openTries: 0
    property int readTries: 0
    property int pendings: 0
    property double createdAt: 0
    property double reloadedAt: 0              // when the page was last loaded again (the give-up clock is the phase's, and keeps running)
    property int misses: 0                    // answers in a row that said the key panel was not there
    property int reloads: 0
    property bool opened: false               // the key panel of the source was asked for
    property string lastLine: ""
    property bool asking: false

    function note_(text) { if (text !== lastLine) { lastLine = text; App.note("source", text) } }
    function enter(next, text) {
        if (next !== phase) { phase = next; phaseSince = Date.now(); note_(next + ": " + text) }
        if (text !== note) note = text
    }
    function fail(why) { failure = why; enter("failed", why) }
    function reset() {
        phase = "idle"; note = ""; failure = ""; sourceName = ""; signedIn = false; found = false; keySaved = false
        phaseSince = 0; reloadedAt = 0; emptyLooks = createTries = openTries = readTries = pendings = misses = reloads = 0; createdAt = 0; opened = false; lastLine = ""
    }
    function onStudio(st) { return view.testPage || st.host === "studio.x.com" }

    function advance(st) {
        const now = Date.now()
        if (st.missing || st.error) { view.inject(); return }
        if (phase === "done" || phase === "failed") return                // finished, or handed over to the person (the box and the paste button are theirs now)
        if (st.signedOut) { signedIn = false; enter("signin", "Sign in to X in the box. MOBS carries on by itself afterwards."); return }
        if (phase === "signin") enter("loading", "Signed in. Opening your sources...")
        if (!onStudio(st)) { if (now - phaseSince > 4000) { view.url = xSourcesUrl; phaseSince = now } return }   // X sent the person to its home page after the password
        const onSources = st.sources > 0 || st.createSourceButton
        if (!onSources) {
            enter("loading", "Opening your sources...")
            if (now - Math.max(phaseSince, reloadedAt) > Math.min(25000, pilotPatienceMs / 2) && reloads < 3) { reloads++; reloadedAt = now; view.url = xSourcesUrl }
            else if (now - phaseSince > pilotPatienceMs) fail("Live Studio did not open for this account. It needs X Premium.")
            return
        }
        if (!signedIn) { signedIn = true; Prefs.setXSignedIn(true) }   // Live Studio's own page is showing, so the sign-in is real (the chat card and the livestream finder rely on it)
        if (st.sources === 0) {                                        // a drawn page with no source on it: made once, here, with a name of MOBS's own
            if (++emptyLooks < 5) { enter("looking", "Looking for your source..."); return }   // the list may still be loading
            if (createTries >= 8) { fail("MOBS could not make a source on X."); return }
            if (now - createdAt < pilotTickMs * 6) return                        // one Create at a time: X needs a moment to draw the new row
            createdAt = now
            emptyLooks = 0                                              // a fresh grace for the new row to appear
            createTries++
            enter("creating", "You have no source on X yet: making one called MOBS...")
            view.ask("createSource", "MOBS", function () {})
            return
        }
        emptyLooks = 0
        found = true
        if (!opened) {
            if (openTries >= 6) { fail("MOBS could not open your source's key."); return }
            openTries++
            enter("reading", "Reading your stream key from X...")
            view.ask("openKey", Prefs.sourceName, function (a) { if (a.ok) { opened = true; if (a.name) sourceName = a.name } })
            return
        }
        if (readTries >= 12) { fail("X would not show the key of your source."); return }
        readTries++
        view.ask("readKey", undefined, function (a) {   // the answer carries the key: it is handed on and forgotten, never logged
            if (!root.running || root.phase !== "reading") return                    // a late answer for a pass that has been reset or handed over
            if (a.pending) { if (++pendings < 6) readTries--; return }                                                       // the key is behind its dots: the panel was asked to show it
            if (!a.ok) { if (++misses >= 3) { misses = 0; opened = false } return }      // the panel is not there (it closed): it is opened again
            const problem = a.key ? Prefs.setStreamKey(a.key) : "no key"
            const where = a.url ? Prefs.setStreamUrl(a.url) : ""
            if (problem !== "") { root.fail("MOBS could not keep the key X showed: " + problem); return }
            if (where !== "") note_("the address X showed was not usable: the one MOBS has stays")
            if (sourceName !== "") Prefs.setSourceName(sourceName)
            keySaved = true
            view.ask("closePanel", undefined, function () {})
            enter("done", "Your source" + (sourceName !== "" ? ", " + sourceName + "," : "") + " and its key are saved." + (where !== "" ? " The address X showed could not be used, so MOBS keeps its own." : ""))
        })
    }

    function step() {
        if (asking || !view) return
        asking = true
        if (!view.ask("look", undefined, function (st) { asking = false; if (root.running) root.advance(st) })) asking = false
    }
    onRunningChanged: { reset(); if (running) enter("loading", "Opening Live Studio...") }
    Timer { interval: pilotTickMs; repeat: true; running: root.running && root.phase !== "done"; onTriggered: root.step() }
}
