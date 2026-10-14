#include "chatmodel.h"
#include "mobslog.h"
#include <QChar>
#include <QClipboard>
#include <QDateTime>
#include <QGuiApplication>
#include <QRegularExpression>

ChatModel::ChatModel(QObject *parent) : QAbstractListModel(parent) {
    m_endTimer.setSingleShot(true);
    m_endTimer.setInterval(qEnvironmentVariableIntValue("MOBS_END_MS") ?: 14000);   // tests shorten it
    connect(&m_endTimer, &QTimer::timeout, this, [this] { MobsLog::write("chat", "ending the livestream on X took too long: giving up"); endFinished(false); });
}

int ChatModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : int(m_items.size()); }

QVariant ChatModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) return {};
    const Item &item = m_items.at(index.row());
    switch (role) {
    case NameRole: return item.name;
    case TextRole: return item.text;
    case HueRole: return item.hue;
    case InitialRole: {   // the first letter, whole: an emoji is two UTF-16 units and must not be cut in half
        const int length = !item.name.isEmpty() && item.name.at(0).isHighSurrogate() ? 2 : 1;
        return item.name.left(length).toUpper();
    }
    }
    return {};
}

QHash<int, QByteArray> ChatModel::roleNames() const {
    return {{NameRole, "name"}, {TextRole, "text"}, {HueRole, "hue"}, {InitialRole, "initial"}};
}

double ChatModel::hueFor(const QString &name) {
    quint32 h = 2166136261u;                                   // FNV-1a: small, stable, spreads short names well
    for (const QChar c : name.toLower()) { h ^= quint32(c.unicode()); h *= 16777619u; }
    return double(h % 3600) / 3600.0;
}

bool ChatModel::add(const QString &id, const QString &name, const QString &text) {
    QString body = text.simplified();                          // a chat line is one paragraph
    if (body.isEmpty()) return false;
    if (!id.isEmpty())
        for (const Item &item : std::as_const(m_items)) if (item.id == id) return false;
    if (body.size() > kTextMax) body = body.left(kTextMax - 1) + QChar(0x2026);
    QString who = name.simplified();
    if (who.isEmpty()) who = QStringLiteral("viewer");
    if (who.size() > kNameMax) who = who.left(kNameMax - 1) + QChar(0x2026);
    if (m_items.size() >= kMax) { beginRemoveRows({}, 0, 0); m_items.removeFirst(); endRemoveRows(); }
    beginInsertRows({}, int(m_items.size()), int(m_items.size()));
    m_items.append(Item{id, who, body, hueFor(who)});
    endInsertRows();
    emit countChanged();
    return true;
}

void ChatModel::setConfigured(bool configured, bool demo) {
    if (configured == m_configured && demo == m_demo) return;
    m_configured = configured; m_demo = demo;
    emit statusChanged();
}

void ChatModel::setStatus(bool connected, const QString &status) {
    if (connected == m_connected && status == m_status) return;
    m_connected = connected; m_status = status;
    emit statusChanged();
}

void ChatModel::clear() {
    if (m_items.isEmpty()) return;
    beginResetModel();
    m_items.clear();
    endResetModel();
    emit countChanged();
}

QString ChatModel::xIdFrom(const QString &text) {
    // x.com or twitter.com with /i/live-studio/ or /i/broadcasts/, or studio.x.com with /live/ (the address of Live Studio itself), then the id.
    // The id must follow a slash or a dot, so "evilx.com/..." never matches.
    static const QRegularExpression re(R"((?:^|[/.])(?:(?:x|twitter)\.com/i/(?:live-studio|broadcasts)|studio\.x\.com/live)/([A-Za-z0-9]{8,24})(?:[/?#\s]|$))");
    const QRegularExpressionMatch m = re.match(text.trimmed());
    return m.hasMatch() ? m.captured(1) : QString();
}

bool ChatModel::attachX(const QString &textOrLink) {
    const QString id = xIdFrom(textOrLink);
    if (id.isEmpty()) return false;
    if (id == m_xId) return true;
    if (m_staleIds.contains(id)) { MobsLog::write("chat", "that livestream was already on air before this broadcast began: not attached"); return false; }
    m_xId = id;
    m_endedOk = false;
    m_trend.clear(); m_totalViews = -1; emit viewersChanged();   // the readings are of THIS livestream
    m_finderNote.clear();
    if (!m_configured) { m_configured = true; m_xConfigured = true; }
    m_demo = false; m_connected = true; m_status.clear();
    MobsLog::write("chat", "X chat attached: the card under the webcam shows the Live Chat of your livestream");   // nothing the person copied is ever logged
    emit statusChanged();
    emit attached(id);
    return true;
}

bool ChatModel::attachXByHand(const QString &textOrLink) {
    const QString id = xIdFrom(textOrLink);
    if (id.isEmpty()) return false;
    if (m_staleIds.removeAll(id) > 0) MobsLog::write("chat", "you gave this livestream yourself: taken although it was on air before the broadcast began");
    return attachX(textOrLink);
}

QString ChatModel::xTypedNote(const QString &text) const {
    const QString typed = text.trimmed();
    if (typed.isEmpty() || !xIdFrom(typed).isEmpty()) return QString();     // nothing typed, or a link that is taken
    return QStringLiteral("That is not a livestream link MOBS knows. In Live Studio open your livestream and copy the address from the browser's address bar instead.");
}

void ChatModel::rememberClipboard() {
    m_endedOk = false;   // a new broadcast begins: what the last one's end said is over
    const QClipboard *clipboard = QGuiApplication::clipboard();
    m_clipboardAtStart = clipboard ? clipboard->text() : QString();
}

void ChatModel::setFinderNote(const QString &note) {
    if (note == m_finderNote) return;
    m_finderNote = note;
    emit statusChanged();
}

bool ChatModel::attachXFromClipboard() {
    const QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard) return false;
    const QString text = clipboard->text();
    if (!text.isEmpty() && text.trimmed() == m_clipboardAtStart.trimmed()) return false;   // it was already there when the broadcast began: an old link, not one copied for this livestream
    const QString id = xIdFrom(text);
    if (!id.isEmpty() && id != m_xId && m_usedIds.contains(id)) return false;               // a livestream of an earlier broadcast, wherever and whenever it was copied
    return attachX(text);
}

QString ChatModel::xClipboardNote() const {
    if (!m_xId.isEmpty()) return QString();
    const QClipboard *clipboard = QGuiApplication::clipboard();
    const QString text = clipboard ? clipboard->text().trimmed() : QString();
    if (text.isEmpty() || text.size() > 400) return QString();
    if (text == m_clipboardAtStart.trimmed()) return QString();         // what was there before the broadcast is not commented on
    const QString id = xIdFrom(text);
    if (!id.isEmpty() && m_usedIds.contains(id)) return QStringLiteral("That livestream is from an earlier broadcast. Use the one you just started.");
    if (!id.isEmpty()) return m_staleIds.contains(id) ? QStringLiteral("That livestream was already on air before this broadcast began. Use the one you just started.") : QString();   // otherwise a link that would have been taken
    static const QRegularExpression xLink(R"((?:^|[/.])(?:x|twitter)\.com/)");
    if (!xLink.match(text).hasMatch()) return QString();                                      // whatever else was copied is none of MOBS's business, and is never shown
    return QStringLiteral("That link is not a livestream link MOBS knows. In Live Studio open your livestream and copy the address from the browser's address bar instead.");
}

void ChatModel::detachX() {
    m_staleIds.clear();   // the next broadcast starts clean
    m_clipboardAtStart.clear();
    if (!m_finderNote.isEmpty()) { m_finderNote.clear(); if (m_xId.isEmpty()) emit statusChanged(); }
    if (m_xId.isEmpty()) return;
    m_xId.clear();
    m_trend.clear(); m_totalViews = -1; emit viewersChanged();
    if (m_xConfigured) { m_configured = false; m_xConfigured = false; m_connected = false; m_status = QStringLiteral("chat is not connected"); }
    emit statusChanged();
}

void ChatModel::noteViewers(int watching, int totalViews) { noteViewersAt(watching, totalViews, QDateTime::currentMSecsSinceEpoch()); }

void ChatModel::noteViewersAt(int watching, int totalViews, qint64 atMs) {
    if (m_xId.isEmpty() || (watching < 0 && totalViews < 0)) return;
    const int before = m_trend.current(), beforeTrend = m_trend.trend(), beforeDelta = m_trend.delta(), beforeTotal = m_totalViews;
    const bool beforeReady = m_trend.ready();
    if (watching >= 0) m_trend.add(watching, atMs);
    if (totalViews >= 0) m_totalViews = totalViews;
    if (before != m_trend.current() || beforeTrend != m_trend.trend() || beforeDelta != m_trend.delta() || beforeTotal != m_totalViews || beforeReady != m_trend.ready()) emit viewersChanged();
}

bool ChatModel::endLivestream() {
    if (m_xId.isEmpty() || m_ending) return false;
    m_ending = true; m_endedOk = false;
    MobsLog::write("chat", "ending the livestream on X");
    m_endTimer.start();
    emit endingChanged();
    emit endRequested();
    return true;
}

void ChatModel::endFinished(bool ok) {
    if (!m_ending) return;
    m_endTimer.stop();
    m_ending = false; m_endedOk = ok;
    MobsLog::write("chat", ok ? "the livestream on X is ended" : "the livestream on X could not be ended from here");
    emit endingChanged();
}
