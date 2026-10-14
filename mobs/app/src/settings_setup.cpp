#include "settings.h"
#include "mobslog.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QUrl>

// The guided setup's part of Settings: where the person stopped, the microphone they locked in, and pasting the stream key. Kept apart from
// settings.cpp so the long list of older settings there stays easy to read.

void Settings::setSetupDone(bool done) {
    if (done == m_setupDone) return;
    m_setupDone = done;
    m_settings->setValue("setupDone", done); m_settings->sync();
    MobsLog::write("setup", done ? "guided setup finished" : "guided setup will run again");
    emit changed();
}

// The name of the person's source on X ("Gus", "MOBS"). X does the real work with it; MOBS only remembers the name so the go-live window can say which one to pick.
QString Settings::cleanSourceName(const QString &name) {
    QString out;
    for (const QChar c : name)
        if (c.category() != QChar::Other_Control && c != QLatin1Char('<') && c != QLatin1Char('>') && c != QLatin1Char('&')) out += c;   // no markup: it is shown as text
    return out.simplified().left(40);
}

void Settings::setSourceName(const QString &name) {
    const QString clean = cleanSourceName(name);
    if (clean == m_sourceName) return;
    m_sourceName = clean;
    m_settings->setValue("sourceName", clean); m_settings->sync();
    emit changed();
}

// The title of the livestream. X's form refuses Go Live until it has one ("Add a title to go live."), which is what stopped Fred's first go-live on 2026-10-13, so MOBS types it
// itself: the person edits it on the ready screen, and it is kept until they change it. Plain text on one line, no markup, at most 100 characters.
QString Settings::cleanLiveTitle(const QString &title) {
    QString out;
    for (const QChar c : title) {
        if (c.isSpace()) out += QLatin1Char(' ');                  // a tab or a line break between two words is a space, not nothing ("Hello\nworld" must not become "Helloworld")
        else if (c.category() != QChar::Other_Control && c != QLatin1Char('<') && c != QLatin1Char('>')) out += c;
    }
    return out.simplified().left(100);
}

void Settings::setLiveTitle(const QString &title) {
    const QString clean = cleanLiveTitle(title);
    if (clean == m_liveTitle) return;
    m_liveTitle = clean;
    m_settings->setValue("liveTitle", clean); m_settings->sync();
    emit changed();
}

void Settings::setLivePublic(bool everyone) {
    if (everyone == m_livePublic) return;
    m_livePublic = everyone;
    m_settings->setValue("livePublic", everyone); m_settings->sync();
    emit changed();
}

void Settings::setSetupStep(int step) {
    step = qBound(0, step, 20);
    if (step == m_setupStep) return;
    m_setupStep = step;
    m_settings->setValue("setupStep", step); m_settings->sync();
    emit changed();
}

// "This is my microphone": the choice is kept until the person picks another one (the lock then follows the new one). A locked microphone that
// is unplugged is reported as unplugged, never swapped for another one behind the person's back.
void Settings::lockMic(const QString &id) {
    if (id.isEmpty()) return;
    if (id != m_audio.micDevice) { m_audio.micDevice = id; m_settings->setValue("micDevice", id); }
    m_mic = true; m_settings->setValue("mic", true);
    m_micLocked = true; m_settings->setValue("micLocked", true);
    m_settings->sync();
    MobsLog::write("setup", "microphone locked in");   // the device name is not needed in the log
    emit changed();
}

void Settings::unlockMic() {
    if (!m_micLocked) return;
    m_micLocked = false; m_settings->setValue("micLocked", false); m_settings->sync();
    emit changed();
}

// The stream key is the one secret in the flow. It comes from the clipboard because the person copied it in Live Studio; it is saved by
// setStreamKey (a private file) and never logged or shown. A web address or a sentence is not a key, and says so in words.
QString Settings::pasteStreamKeyFromClipboard() {
    const QClipboard *clipboard = QGuiApplication::clipboard();
    const QString text = clipboard ? clipboard->text().trimmed() : QString();
    if (text.isEmpty()) return "Nothing to paste yet. In Live Studio, copy the Stream Key first, then press this button again.";
    if (text.contains("://") || text.contains(QRegularExpression("\\s")))
        return "That is not a stream key. In Live Studio click the key icon and copy the one called Stream Key, not the Server URL.";
    return setStreamKey(text);
}

void Settings::setXSignedIn(bool in) {
    if (in == m_xSignedIn) return;
    m_xSignedIn = in;
    m_settings->setValue("xSignedIn", in); m_settings->sync();
    MobsLog::write("setup", in ? "signed in to X inside MOBS" : "not signed in to X inside MOBS");
    emit changed();
}

// X sends a signed-in person to its home page; the sign-in browser reports every page it shows, and this notices that one.
bool Settings::xHomeUrl(const QString &url) {
    const QUrl u(url);
    return u.scheme() == "https" && u.host() == "x.com" && (u.path() == "/home" || u.path().startsWith("/home/"));
}

void Settings::noteXUrl(const QString &url) { if (xHomeUrl(url)) setXSignedIn(true); }
