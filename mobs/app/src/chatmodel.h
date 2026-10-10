#pragma once
#include <QAbstractListModel>
#include <QString>
#include <QVector>

// The live chat of the broadcast as a flow of messages next to the webcam. Read-only on purpose: you answer with your voice and your camera.
// Newest last. A feed (X's official chat relay, or the demo) calls add(); this class only keeps and shows what it is given, and ignores a
// message it has already seen (a feed may resend after it reconnects).
class ChatModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool configured READ configured NOTIFY statusChanged)   // a chat feed exists at all: without one there is no chat panel under the webcam
    Q_PROPERTY(bool connected READ connected NOTIFY statusChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)         // what to tell the person while there is nothing to show
    Q_PROPERTY(bool demo READ demo NOTIFY statusChanged)                 // the messages are made up (MOBS_FAKE_CHAT): the panel says so
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Role { NameRole = Qt::UserRole + 1, TextRole, HueRole, InitialRole };
    static constexpr int kMax = 200, kNameMax = 32, kTextMax = 400;
    explicit ChatModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    bool configured() const { return m_configured; }
    bool connected() const { return m_connected; }
    bool demo() const { return m_demo; }
    QString status() const { return m_status; }
    int count() const { return int(m_items.size()); }
    // One message from the feed. False when it was ignored: nothing to show, or this id was already added.
    bool add(const QString &id, const QString &name, const QString &text);
    void setConfigured(bool configured, bool demo = false);
    void setStatus(bool connected, const QString &status);
    Q_INVOKABLE void clear();
    static double hueFor(const QString &name);   // the same name always gets the same colour: 0..1
signals:
    void statusChanged();
    void countChanged();
private:
    struct Item { QString id, name, text; double hue; };
    QVector<Item> m_items;
    bool m_configured = false, m_connected = false, m_demo = false;
    QString m_status = QStringLiteral("chat is not connected");
};
