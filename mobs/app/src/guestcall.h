#pragma once
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <functional>

// A viewer on a call with you, shown as the guest card ABOVE your webcam in the dock. The call itself is a Jitsi room (meet.jit.si) that a web view
// inside the card opens; the viewer opens the same room in their own browser, with their camera and microphone or just the microphone, and never
// shares a screen. You keep sharing yours. This class is only the bookkeeping: it makes the room, writes the invite onto YOUR clipboard (MOBS never
// messages anybody: you paste it to the viewer yourself), follows what the page says about who is in the room, and keeps a sign-in window from
// ever opening while a take runs (viewers would watch you type a password).
// Their voice reaches the broadcast as desktop sound, because the web view plays it like any other app. Your camera cannot be in the call (a webcam
// belongs to one program at a time, and MOBS holds it for the dock), so the guest hears you and sees you only the way the viewers do.
class GuestCall : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)          // "idle" | "waiting" (the room is open, nobody else is in it) | "connected"
    Q_PROPERTY(QString guest READ guest NOTIFY changed)          // the viewer you picked from the chat, if you did
    Q_PROPERTY(QString label READ label NOTIFY changed)          // who the card names: the person in the room, else the one you invited, else "guest"
    Q_PROPERTY(QString status READ status NOTIFY changed)        // one line for the card
    Q_PROPERTY(QUrl url READ url NOTIFY changed)                 // what the card's web view opens (YOUR side of the room)
    Q_PROPERTY(bool pageReady READ pageReady NOTIFY changed)     // the page has loaded: the card shows it instead of the placeholder
    Q_PROPERTY(bool joined READ joined NOTIFY changed)           // you are in the room (false while Jitsi still waits for a signed-in host)
    Q_PROPERTY(bool standIn READ standIn CONSTANT)               // tests and screenshots: no web page, no clipboard, no desktop notice
    Q_PROPERTY(QString pageScript READ viewScript CONSTANT)      // injected into the room's page: reports who is in it
    Q_PROPERTY(bool debug READ debug CONSTANT)                   // MOBS_CALL_DEBUG=1: the page's clicks, warnings and errors are written to the log
    Q_PROPERTY(QString lastInvite READ lastInvite NOTIFY changed) // the invite text as it was put on the clipboard (tests read it)
public:
    static constexpr int kRoomChars = 20;
    explicit GuestCall(bool standIn, QObject *parent = nullptr);
    bool active() const { return !m_room.isEmpty(); }
    QString state() const;
    QString guest() const { return m_guest; }
    QString label() const;
    QString status() const;
    QUrl url() const;   // MOBS_CALL_URL replaces it (tests: an offline page); never set it in real use
    bool pageReady() const { return m_pageReady; }
    bool joined() const { return m_joined; }
    bool standIn() const { return m_standIn; }
    QString lastInvite() const { return m_lastInvite; }
    static QString pageScript(bool debug = false);
    QString viewScript() const { return pageScript(debug()); }
    bool debug() const { return qEnvironmentVariable("MOBS_CALL_DEBUG") == "1"; }
    Q_INVOKABLE void pageSaid(const QString &what);              // debug: something the page itself reported (a click on it, say): logged, and the last one kept for tests
    QString lastPageSaid() const { return m_pageSaid; }
    // A running take (recording or broadcasting) is what the sign-in guard looks at.
    void setTakeProbe(std::function<bool()> probe) { m_busy = std::move(probe); }
    void takeChanged();

    // Start a call (a viewer's name is optional) and put the invite on the clipboard. A call that is already open just copies its invite again.
    Q_INVOKABLE void invite(const QString &viewer = QString());
    Q_INVOKABLE void copyInvite();
    Q_INVOKABLE void end();
    Q_INVOKABLE void report(const QString &json);                 // the page's own report: who is in the room
    Q_INVOKABLE void pageLoaded(bool ok, const QString &problem); // the web view finished loading (or could not)
    Q_INVOKABLE bool signInAllowed();                              // a popup (the Jitsi host sign-in) asks to open: false while a take runs
    Q_INVOKABLE void log(const QString &line);

    // Pure helpers (unit-tested).
    static QString newRoom();                                      // "mobs-" and 20 characters nobody can guess
    static bool validRoom(const QString &room);
    static QUrl guestUrl(const QString &room);                     // the link the viewer opens: no settings, they pick their camera and microphone
    static QUrl hostUrl(const QString &room, const QString &hostName);   // your side: no pre-join page, camera muted, no invite buttons, no deep-link page
    static QString inviteText(const QUrl &guestLink, const QString &viewer);
    static QString hostName();                                     // the first name of this account, or "Host"
    static QString cleanName(const QString &name);                 // what a viewer's display name may look like on the card
    static bool parseReport(const QString &json, bool *joined, QStringList *others);   // false: not a report this version understands
signals:
    void changed();
private:
    void touch() { emit changed(); }
    static bool copyToClipboard(const QString &text);
    QString m_room, m_guest, m_blockedNote, m_loadError, m_lastInvite, m_pageSaid;
    QStringList m_others;
    bool m_joined = false, m_pageReady = false, m_hadGuest = false, m_standIn;
    std::function<bool()> m_busy;
};
