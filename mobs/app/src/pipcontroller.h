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
public:
    static inline const QString kTitle = QStringLiteral("mobs webcam");
    PipController(AppState *state, Engine *engine, Settings *prefs, QSettings *store, QObject *parent = nullptr);
    ~PipController() override;
    bool wanted() const;
    QCameraDevice device() const;
    QCameraFormat format() const;
    bool testPattern() const;
    QString title() const { return kTitle; }
    Q_INVOKABLE void windowShown();                     // the QML window became visible: float, pin and place it
    Q_INVOKABLE void windowHidden();                    // it is about to go: remember where it was left, if the person moved or resized it
    Q_INVOKABLE void cameraProblem(const QString &what); // the camera view's own messages, for the log
    void setCaptureSize(int width, int height);         // the captured screen's size once known: which monitor is in the picture

    // Pure helpers over `hyprctl ... -j` output (unit-tested).
    static QString addressOf(const QJsonArray &clients, const QString &title, qint64 pid);   // OUR window only: title AND process id
    static QJsonObject clientAt(const QJsonArray &clients, const QString &address);
    static QJsonObject monitorById(const QJsonArray &monitors, int id);
    static QJsonObject monitorByName(const QJsonArray &monitors, const QString &name);
    static QSize defaultSize(const QJsonObject &monitor);                                        // 20% of the usable width (480..1024): visible once the broadcast is scaled to 1080p
    static QRect defaultRect(const QJsonObject &monitor, const QSize &size, int margin = 24);   // top-right of the monitor's usable area
    static QRect fitInside(const QRect &want, const QJsonObject &monitor);                      // a remembered place that still lies on the monitor
    static QString encodePlace(const QRect &rect, const QString &monitor);
    static bool decodePlace(const QString &text, QRect *rect, QString *monitor);
    static bool shouldHide(const QJsonObject &monitor, const QSize &captured, int monitorCount); // is MOBS's window on the monitor that is in the picture?
signals:
    void changed();
private:
    bool onHyprland() const;
    QJsonDocument hyprJson(const QStringList &args) const;
    bool hyprEval(const QString &lua) const;
    void rememberActive();
    void tryPlace();
    void place(const QString &address, const QJsonArray &clients);
    void onEngineState();
    void hideMain();
    void restoreMain();
    AppState *m_state; Engine *m_engine; Settings *m_prefs; QSettings *m_store;
    QTimer m_placeTimer;
    int m_tries = 0;
    bool m_shown = false;
    QString m_prevActive;     // what had the keyboard when the window was about to appear
    QRect m_placed;           // where WE put the window: only a different place is worth remembering
    QSize m_captureSize;
    bool m_mainHidden = false;
    QString m_hiddenAddress, m_homeWorkspace;
};
