import QtQuick

// A Loader that lets go of what it loaded a moment AFTER it is told to, for the web views of X. A web view that is destroyed while a question to its page is still on the way
// crashes Qt WebEngine: the answer arrives after the view's QML context is gone and the engine looks into nothing (seen in a test: SIGSEGV in
// QQuickWebEngineViewPrivate::didRunJavaScript when a take ended and the chat was detached, one run in eight). `wanted` is what the rest of MOBS sets. The helpers inside stop asking
// the moment it is false (they read it too), and the view goes `lingerMs` later, long after any answer that was still coming has arrived.
Loader {
    id: root
    property bool wanted: false
    property bool lingering: false
    property int lingerMs: 2500
    active: wanted || lingering
    onWantedChanged: {
        if (wanted) { lingering = false; letGo.stop() }
        else if (item) { lingering = true; letGo.restart() }   // only something that was really loaded needs time: lingering must never create the view
    }
    Timer { id: letGo; interval: root.lingerMs; onTriggered: root.lingering = false }
}
