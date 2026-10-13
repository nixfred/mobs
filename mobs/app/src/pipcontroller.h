#pragma once
#include <QCameraDevice>
#include <QCameraFormat>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QRect>
#include <QSettings>
#include <QSize>
#include <QTimer>

class AppState;
class ChatModel;
class Engine;
class Settings;

// "Screen + me" as a picture-in-picture: the webcam is a real window ON the screen, pinned to every workspace so it follows you around, and MOBS
// captures the ENTIRE screen, which therefore contains it. You move it the way you move any floating window: SUPER + left-drag (Omarchy's own
// binding), at any time, even in the middle of a broadcast and while you work in another app. This class decides when the window is wanted,
// finds the camera for it, and (on Hyprland) floats, pins, sizes and places it, then hands the keyboard focus back to what you were doing.
// It also gets MOBS's OWN window out of the picture while a take records (otherwise the stream shows MOBS showing the stream).
class PipController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool wanted READ wanted NOTIFY changed)
    Q_PROPERTY(QCameraDevice device READ device NOTIFY changed)
    Q_PROPERTY(QCameraFormat format READ format NOTIFY changed)
    Q_PROPERTY(bool testPattern READ testPattern NOTIFY changed)   // a stand-in picture instead of a camera: tests and screenshots
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(bool mainHeld READ mainHeld NOTIFY heldChanged)     // MOBS's own window is kept up while the person finishes going live in X
    Q_PROPERTY(bool chatWanted READ chatWanted NOTIFY changed)     // the webcam window carries the chat card under the video
public:
    static inline const QString kTitle = QStringLiteral("mobs webcam");
    PipController(AppState *state, Engine *engine, Settings *prefs, QSettings *store, QObject *parent = nullptr);
    ~PipController() override;
    bool wanted() const;
    QCameraDevice device() const;
    QCameraFormat format() const;
    bool testPattern() const;
    bool chatWanted() const;
    void setChat(ChatModel *chat);                      // the live chat shown under the webcam, when there is a feed for it
    QString title() const { return kTitle; }
    bool mainHeld() const { return m_held; }
    // The person pressed "I'm live" (or the minutes ran out): now MOBS's window steps out of the picture, as it always did for a plain recording.
    Q_INVOKABLE void hideMainNow();
    Q_INVOKABLE void windowShown();                     // the QML window became visible: float, pin and place it
    Q_INVOKABLE void windowHidden();                    // it is about to go: remember where it was left, if the person moved or resized it
    Q_INVOKABLE void cameraProblem(const QString &what); // the camera view's own messages, for the log
    void setCaptureSize(int width, int height);         // the captured screen's size once known: which monitor is in the picture

    // Pure helpers over `hyprctl ... -j` output (unit-tested).
    static QString addressOf(const QJsonArray &clients, const QString &title, qint64 pid);   // OUR window only: title AND process id
    static QJsonObject clientAt(const QJsonArray &clients, const QString &address);
    static QJsonObject monitorById(const QJsonArray &monitors, int id);
    static QJsonObject monitorByName(const QJsonArray &monitors, const QString &name);
    static int chatHeightFor(int width);   // the chat card's height under a video that wide
    static QSize defaultSize(const QJsonObject &monitor, bool withChat = false);                                        // 20% of the usable width (480..1024): visible once the broadcast is scaled to 1080p
    static QRect defaultRect(const QJsonObject &monitor, const QSize &size, int margin = 24);   // top-right of the monitor's usable area
    static QRect fitInside(const QRect &want, const QJsonObject &monitor);                      // a remembered place that still lies on the monitor
    static QRect rectForChat(const QRect &now, bool chatOn, const QJsonObject &monitor);        // the window's rect when the chat card turns on or off: same corner and width
    static QString encodePlace(const QRect &rect, const QString &monitor);
    static bool decodePlace(const QString &text, QRect *rect, QString *monitor);
    static bool shouldHide(const QJsonObject &monitor, const QSize &captured, int monitorCount); // is MOBS's window on the monitor that is in the picture?
    // A webcam window closed in the middle of a take (a stray SUPER + Q) takes you off the stream while the take carries on. During a take MOBS puts it back,
    // at most three times a minute so it never fights a person who really means to close it. Before and after a take a closed window stays closed.
    static bool reopenAllowed(const QString &engineState, int reopensInTheLastMinute);
signals:
    void changed();
    void heldChanged();
    void reopenRequested();                            // the window was closed during a take: the QML window should show itself again
private:
    void holdMain();
    void releaseHold();
    void rememberWhereItWasLeft();
    void reopenIfClosedDuringATake();
    QTimer m_holdTimer;                                // a few minutes after going live MOBS's window steps out of the picture even if nobody pressed "I'm live"
    bool m_held = false;
    QList<qint64> m_reopens;                           // when MOBS put the window back (ms since epoch): the last minute decides whether to keep doing it
    qint64 m_reopenAskedAt = 0;                        // a put-back was asked for then: the window showing again within a few seconds is that put-back
    bool m_gaveUp = false;                             // closed again and again: said so once, then left alone until the next take
    bool onHyprland() const;
    QJsonDocument hyprJson(const QStringList &args) const;
    bool hyprEval(const QString &lua) const;
    void rememberActive();
    void tryPlace();
    void resizeForChat();   // the chat card turned on or off while the window is on screen
    void place(const QString &address, const QJsonArray &clients);
    void onEngineState();
    void hideMain();
    void restoreMain();
    AppState *m_state; Engine *m_engine; Settings *m_prefs; QSettings *m_store;
    QTimer m_placeTimer;
    int m_tries = 0;
    ChatModel *m_chat = nullptr;
    QString m_placeKey = QStringLiteral("pipPlace");   // where this window's remembered place lives: the dock with chat has its own, the video alone keeps its own
    bool m_shown = false;
    QString m_prevActive;     // what had the keyboard when the window was about to appear
    QRect m_placed;           // where WE put the window: only a different place is worth remembering
    bool m_chatPlaced = false; // does the window's current size have room for the chat card?
    QSize m_captureSize;
    bool m_mainHidden = false;
    QString m_hiddenAddress, m_homeWorkspace;
};
