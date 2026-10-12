#include "chatmodel.h"
#include "mobslog.h"
#include <QChar>
#include <QClipboard>
#include <QGuiApplication>
#include <QRegularExpression>

ChatModel::ChatModel(QObject *parent) : QAbstractListModel(parent) {}

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
    m_xId = id;
    if (!m_configured) { m_configured = true; m_xConfigured = true; }
    m_demo = false; m_connected = true; m_status.clear();
    MobsLog::write("chat", "X chat attached: the card under the webcam shows the Live Chat of your livestream");   // never the link itself
    emit statusChanged();
    return true;
}

bool ChatModel::attachXFromClipboard() {
    const QClipboard *clipboard = QGuiApplication::clipboard();
    return clipboard && attachX(clipboard->text());
}

QString ChatModel::xClipboardNote() const {
    if (!m_xId.isEmpty()) return QString();
    const QClipboard *clipboard = QGuiApplication::clipboard();
    const QString text = clipboard ? clipboard->text().trimmed() : QString();
    if (text.isEmpty() || text.size() > 400 || !xIdFrom(text).isEmpty()) return QString();   // nothing copied, something huge, or a link that would have been taken
    static const QRegularExpression xLink(R"((?:^|[/.])(?:x|twitter)\.com/)");
    if (!xLink.match(text).hasMatch()) return QString();                                      // whatever else was copied is none of MOBS's business, and is never shown
    return QStringLiteral("That link is not a livestream link MOBS knows. In Live Studio open your livestream and copy the address from the browser's address bar instead.");
}

void ChatModel::detachX() {
    if (m_xId.isEmpty()) return;
    m_xId.clear();
    if (m_xConfigured) { m_configured = false; m_xConfigured = false; m_connected = false; m_status = QStringLiteral("chat is not connected"); }
    emit statusChanged();
}
