#include "guestcall.h"
#include "mobslog.h"
#include "notifier.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStandardPaths>
#include <pwd.h>
#include <unistd.h>

GuestCall::GuestCall(bool standIn, QObject *parent) : QObject(parent), m_standIn(standIn) {}

QUrl GuestCall::url() const {
    if (!active()) return {};
    if (const QString test = qEnvironmentVariable("MOBS_CALL_URL"); !test.isEmpty()) return QUrl(test);
    return hostUrl(m_room, hostName());
}

QString GuestCall::state() const {
    if (!active()) return QStringLiteral("idle");
    return m_others.isEmpty() ? QStringLiteral("waiting") : QStringLiteral("connected");
}

QString GuestCall::label() const {
    if (!m_others.isEmpty()) return m_others.size() == 1 ? m_others.first() : QString("%1 and %2 more").arg(m_others.first()).arg(m_others.size() - 1);
    return m_guest.isEmpty() ? QStringLiteral("guest") : m_guest;
}

// One line under the guest's name while their picture is not there yet (and the same line is logged). Most urgent first.
QString GuestCall::status() const {
    if (!active()) return {};
    if (!m_blockedNote.isEmpty()) return m_blockedNote;
    if (!m_loadError.isEmpty()) return m_loadError;
    if (!m_others.isEmpty()) return QStringLiteral("in the call with you");
    if (!m_standIn && !m_pageReady) return QStringLiteral("opening the call...");
    if (!m_standIn && !m_joined) return QStringLiteral("sign in as the host if Jitsi asks, then your guest can come in");
    const QString who = m_guest.isEmpty() ? QStringLiteral("your guest") : m_guest;
    if (m_hadGuest) return QString("%1 left: the room is still open").arg(who);
    return QString("waiting for %1: the invite is on your clipboard, paste it to them").arg(who);
}

void GuestCall::invite(const QString &viewer) {
    if (active()) { copyInvite(); return; }
    m_room = newRoom();
    m_guest = cleanName(viewer);
    m_others.clear(); m_joined = false; m_pageReady = false; m_hadGuest = false; m_blockedNote.clear(); m_loadError.clear();
    MobsLog::write("call", QString("call opened%1: room %2...").arg(m_guest.isEmpty() ? QString() : QString(" for \"%1\"").arg(m_guest), m_room.left(9)));
    copyInvite();   // emits changed
}

void GuestCall::copyInvite() {
    if (!active()) return;
    m_lastInvite = inviteText(guestUrl(m_room), m_guest);
    const bool ok = (m_standIn || qEnvironmentVariable("MOBS_NO_CLIPBOARD") == "1") ? true : copyToClipboard(m_lastInvite);   // a test never touches the real clipboard
    MobsLog::write("call", ok ? QString("the invite is on the clipboard") : QString("could not put the invite on the clipboard (is wl-copy installed?)"));
    if (!ok) m_loadError = QStringLiteral("could not reach the clipboard: send them this link yourself, it is in the log");
    touch();
}

void GuestCall::end() {
    if (!active()) return;
    MobsLog::write("call", "call ended");
    m_room.clear(); m_guest.clear(); m_others.clear(); m_joined = false; m_pageReady = false; m_hadGuest = false; m_blockedNote.clear(); m_loadError.clear();
    touch();
}

bool GuestCall::parseReport(const QString &json, bool *joined, QStringList *others) {
    const QJsonObject o = QJsonDocument::fromJson(json.toUtf8()).object();
    if (o.value("v").toInt() != 1 || !o.value("joined").isBool() || !o.value("others").isArray()) return false;
    *joined = o.value("joined").toBool();
    others->clear();
    for (const QJsonValue &v : o.value("others").toArray()) {
        const QString n = cleanName(v.toObject().value("name").toString());
        others->append(n.isEmpty() ? QStringLiteral("guest") : n);
    }
    return true;
}

void GuestCall::report(const QString &json) {
    if (!active()) return;
    bool joined = false; QStringList others;
    if (!parseReport(json, &joined, &others)) { MobsLog::write("call", "the page said something this version does not understand"); return; }
    if (joined == m_joined && others == m_others) return;
    const QStringList before = m_others;
    m_joined = joined; m_others = others;
    if (m_blockedNote.isEmpty() == false && joined) m_blockedNote.clear();   // they signed in after all
    MobsLog::write("call", joined ? QString("in the room with %1 other%2").arg(others.size()).arg(others.size() == 1 ? "" : "s") : QString("not in the room yet"));
    if (before.isEmpty() && !others.isEmpty()) {
        m_hadGuest = true;
        if (m_guest.isEmpty() && others.size() == 1) m_guest = others.first();
        if (!m_standIn) Notifier::send(QString("MOBS: %1 joined your call").arg(others.first()), "Their voice reaches the broadcast as desktop sound.");
    }
    touch();
}

void GuestCall::pageLoaded(bool ok, const QString &problem) {
    if (!active()) return;
    m_pageReady = ok;
    m_loadError = ok ? QString() : QString("could not open the call: %1").arg(problem.simplified().left(120));
    if (!ok) MobsLog::write("call", "the call page did not load: " + problem.simplified().left(200));
    touch();
}

bool GuestCall::signInAllowed() {
    if (m_busy && m_busy()) {
        m_blockedNote = QStringLiteral("sign in to Jitsi BEFORE you start a take: viewers would see your login");
        MobsLog::write("call", "the Jitsi sign-in window was refused because a take is running (viewers would see it)");
        touch();
        return false;
    }
    if (!m_blockedNote.isEmpty()) { m_blockedNote.clear(); touch(); }
    MobsLog::write("call", "the Jitsi sign-in window opens (no take is running)");
    return true;
}

void GuestCall::takeChanged() {
    if (!m_blockedNote.isEmpty() && !(m_busy && m_busy())) { m_blockedNote.clear(); touch(); }
}

void GuestCall::log(const QString &line) { MobsLog::write("call", line.simplified().left(300)); }

void GuestCall::pageSaid(const QString &what) {
    m_pageSaid = what.simplified().left(200);
    MobsLog::write("call", "page: " + m_pageSaid);
}

// ---------------------------------------------------------------- pure helpers

QString GuestCall::newRoom() {
    static const char alphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";   // no i, l, o, 0 or 1: nothing to misread when a viewer types the link
    QString room = QStringLiteral("mobs-");
    for (int i = 0; i < kRoomChars; ++i) room += QLatin1Char(alphabet[QRandomGenerator::system()->bounded(int(sizeof(alphabet)) - 1)]);
    return room;   // about 99 bits: the room name is the only thing keeping a stranger out, so it must not be guessable
}

bool GuestCall::validRoom(const QString &room) {
    static const QRegularExpression re(QStringLiteral("^mobs-[a-hjkmnp-z2-9]{%1}$").arg(kRoomChars));
    return re.match(room).hasMatch();
}

QUrl GuestCall::guestUrl(const QString &room) { return QUrl(QStringLiteral("https://meet.jit.si/") + room); }

QUrl GuestCall::hostUrl(const QString &room, const QString &hostName) {
    const QString fragment = QStringLiteral(
        "config.prejoinConfig.enabled=false"          // straight into the room: the card is too small for a pre-join page
        "&config.startWithVideoMuted=true"            // your camera belongs to the dock; the call page must never ask for it
        "&config.disableDeepLinking=true"             // no "open the app" page on a narrow layout
        "&config.disableInviteFunctions=true"         // inviting is MOBS's job: the link goes on your clipboard
        "&config.disableThirdPartyRequests=true"      // no avatar lookups or call statistics
        "&config.disableSelfView=true"                // your own tile (a plain avatar, since your camera is off) would take room from the guest
        "&userInfo.displayName=%22") + QString::fromLatin1(QUrl::toPercentEncoding(cleanName(hostName))) + QStringLiteral("%22");
    QUrl url(QStringLiteral("https://meet.jit.si/") + room);
    url.setFragment(fragment);   // already percent-encoded where it needs to be
    return url;
}

QString GuestCall::inviteText(const QUrl &guestLink, const QString &viewer) {
    const QString who = cleanName(viewer);
    return QString("%1Come talk with me live on X: %2\nYou can join with your camera and microphone, or with just your microphone. You will not share your screen, and you can leave any time.")
        .arg(who.isEmpty() ? QString() : QString("Hi %1! ").arg(who), guestLink.toString());
}

QString GuestCall::hostName() {
    const passwd *pw = getpwuid(getuid());
    QString name = pw ? QString::fromLocal8Bit(pw->pw_gecos).section(',', 0, 0).section(' ', 0, 0) : QString();
    name = cleanName(name);
    return name.isEmpty() ? QStringLiteral("Host") : name;
}

QString GuestCall::cleanName(const QString &name) {
    static const QRegularExpression stray(QStringLiteral("[^\\p{L}\\p{N} _.@-]"));   // no quote, backslash, slash or control character may reach a link or a JSON string
    QString n = name.simplified();
    n.remove(stray);
    return n.simplified().left(32);
}

bool GuestCall::copyToClipboard(const QString &text) {
    const QString wlCopy = QStandardPaths::findExecutable("wl-copy");
    if (!wlCopy.isEmpty()) {
        QProcess p;
        p.start(wlCopy, {"--type", "text/plain"});
        if (p.waitForStarted(2000)) {
            p.write(text.toUtf8()); p.closeWriteChannel();
            if (p.waitForFinished(2500) && p.exitCode() == 0) return true;   // wl-copy hands the text to a helper and exits: done
        }
    }
    if (QClipboard *c = QGuiApplication::clipboard()) { c->setText(text); return true; }
    return false;
}

// Runs inside the room's page. It reports, whenever it changes, whether you are in the room and who else is: Jitsi keeps that in a page global.
// Nothing is sent anywhere; the page logs one line that MOBS reads. If Jitsi changes that global, the report stops and the card simply keeps its
// waiting line while the page itself still shows the guest.
QString GuestCall::pageScript(bool debug) {
    QString js = QStringLiteral(R"JS((function () {
  if (window.__mobsCall) return;
  window.__mobsCall = true;
  var last = "";
  function say(o) { var s = JSON.stringify(o); if (s !== last) { last = s; console.log("MOBS-CALL " + s); } }
  function tick() {
    try {
      var conf = window.APP && window.APP.conference;
      if (!conf || !conf.isJoined || !conf.isJoined()) { say({ v: 1, joined: false, others: [] }); return; }
      var members = conf.listMembers ? conf.listMembers() : [];
      say({ v: 1, joined: true, others: members.map(function (m) { return { name: (m.getDisplayName && m.getDisplayName()) || "" }; }) });
    } catch (e) { say({ v: 1, joined: false, others: [] }); }
  }
  setInterval(tick, 1500);
  tick();
})();)JS");
    if (debug) js += QStringLiteral(R"JS(
document.addEventListener("click", function (e) {
  var t = e.target, what = "";
  try { what = String((t.getAttribute && (t.getAttribute("aria-label") || t.getAttribute("class"))) || t.textContent || "").slice(0, 40); } catch (x) {}
  console.log("MOBS-CALL-DEBUG click " + e.clientX + "," + e.clientY + " in a " + window.innerWidth + "x" + window.innerHeight + " page on " + (t && t.tagName) + " " + what);
}, true);
)JS");
    return js;
}
