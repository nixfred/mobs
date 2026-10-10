#include "chatmodel.h"
#include <QChar>

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
