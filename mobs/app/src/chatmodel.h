#pragma once
#include "viewertrend.h"
#include <QAbstractListModel>
#include <QTimer>
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
    Q_PROPERTY(QString xId READ xId NOTIFY statusChanged)                // the X livestream whose chat page the card shows ("" = the message list)
    Q_PROPERTY(QString finderNote READ finderNote NOTIFY statusChanged)   // what the livestream finder has to tell the person ("" = nothing): it gave up, or two livestreams are on air
    Q_PROPERTY(int viewers READ viewers NOTIFY viewersChanged)           // people watching right now, read from the livestream's page (-1 until the page has said)
    Q_PROPERTY(int viewerTrend READ viewerTrend NOTIFY viewersChanged)   // +1 more than two minutes ago, -1 fewer, 0 about the same or too early to say
    Q_PROPERTY(int viewerDelta READ viewerDelta NOTIFY viewersChanged)   // how many more (or fewer) than about two minutes ago
    Q_PROPERTY(bool viewerTrendKnown READ viewerTrendKnown NOTIFY viewersChanged)   // the readings reach back far enough to say (a steady arrow means something only then)
    Q_PROPERTY(int totalViews READ totalViews NOTIFY viewersChanged)     // everyone who has watched any of it so far (-1 unknown)
    Q_PROPERTY(bool ending READ ending NOTIFY endingChanged)              // the livestream is being ended on X (the kill switch, or a stop of a broadcast)
    Q_PROPERTY(bool endedOk READ endedOk NOTIFY endingChanged)            // the last end worked: X says the livestream is over
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
    QString xId() const { return m_xId; }
    // The chat of the person's own livestream, as X shows it to them: the card loads that page (signed in, in MOBS's own browser) and shows only its
    // Live Chat card. The link comes from "Copy URL" in Live Studio; MOBS never has to guess it.
    Q_INVOKABLE bool attachX(const QString &textOrLink);        // true when a Live Studio or broadcast link was found in the text and attached
    Q_INVOKABLE bool attachXFromClipboard();                    // the same, from what the person just copied
    Q_INVOKABLE QString xClipboardNote() const;                 // what to tell the person when they copied an X link that is not a livestream link; "" when there is nothing to say
    // What the person typed or pasted themselves into the go-live window. They said which livestream it is, so even one that was on air before the broadcast began is taken
    // (the list of those can be wrong: a snapshot taken after Go Live holds the new livestream). The finder and the clipboard keep refusing them.
    Q_INVOKABLE bool attachXByHand(const QString &textOrLink);
    Q_INVOKABLE QString xTypedNote(const QString &text) const;  // why typed text was not taken, in words; "" for nothing typed
    // Livestreams that were attached in EARLIER broadcasts (main.cpp keeps the list across runs). The address of one of them that is still on the clipboard is an old one,
    // wherever it was copied: it is never picked up from the clipboard again (the finder never meets them, and a typed link is always taken).
    void setUsedIds(const QStringList &ids) { m_usedIds = ids; }
    QString finderNote() const { return m_finderNote; }
    Q_INVOKABLE void setFinderNote(const QString &note);        // the finder tells the person something (cleared when a livestream is attached or the take ends)
    // What is on the clipboard when a broadcast begins. A link that was ALREADY there is an old one (an earlier livestream's address), never this broadcast's: the
    // go-live window picks a copied link up by itself, and without this it could attach the chat of a livestream that ended days ago.
    void rememberClipboard();
    // How many people are watching, as the page of the livestream says ("3 viewers watching now"), and which way that is going. The dock window shows it beside the webcam.
    int viewers() const { return m_trend.current(); }
    int viewerTrend() const { return m_trend.trend(); }
    int viewerDelta() const { return m_trend.delta(); }
    bool viewerTrendKnown() const { return m_trend.ready(); }
    int totalViews() const { return m_totalViews; }
    Q_INVOKABLE void noteViewers(int watching, int totalViews);      // a reading from the page (-1 for a number it did not show)
    void noteViewersAt(int watching, int totalViews, qint64 atMs);   // the same, at a given time: for the tests
    // End the livestream on X: the chat view of its page presses End livestream and confirms (it is the page of THAT livestream, signed in as the person). False when no livestream
    // is attached or one is being ended already. The answer comes back through endFinished (and after 14 seconds at the latest, whatever the page does).
    Q_INVOKABLE bool endLivestream();
    Q_INVOKABLE void endFinished(bool ok);
    bool ending() const { return m_ending; }
    bool endedOk() const { return m_endedOk; }
    Q_INVOKABLE void detachX();
    Q_INVOKABLE void setStaleIds(const QStringList &ids) { m_staleIds = ids; }   // livestreams that were already on air before this broadcast began: never attached
    bool isStale(const QString &id) const { return m_staleIds.contains(id); }
    static QString xIdFrom(const QString &text);                // "https://x.com/i/live-studio/1PJqrNXnQywxb" gives "1PJqrNXnQywxb"; "" when it is not such a link
    static double hueFor(const QString &name);   // the same name always gets the same colour: 0..1
signals:
    void statusChanged();
    void countChanged();
    void attached(const QString &id);   // a livestream's chat was attached (by the finder, a copied link or a typed one)
    void viewersChanged();
    void endingChanged();
    void endRequested();                // the chat view should end the livestream on X now
private:
    struct Item { QString id, name, text; double hue; };
    QVector<Item> m_items;
    bool m_configured = false, m_connected = false, m_demo = false;
    QString m_status = QStringLiteral("chat is not connected");
    QString m_xId;
    QStringList m_staleIds, m_usedIds;
    QString m_finderNote, m_clipboardAtStart;
    ViewerTrend m_trend;
    int m_totalViews = -1;
    bool m_ending = false, m_endedOk = false;
    QTimer m_endTimer;
    bool m_xConfigured = false;       // the card exists only because an X chat is attached
};
