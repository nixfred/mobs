#pragma once
#include "recorderbackend.h"
#include <QElapsedTimer>
#include <QProcess>
#include <QObject>
#include <QTimer>

// MOBS's recording state machine, for QML. It knows nothing about libobs: it drives a RecorderBackend.
// idle -> starting -> recording <-> paused -> stopping -> idle
class Engine : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    Q_PROPERTY(QString elapsedText READ elapsedText NOTIFY elapsedChanged)
    Q_PROPERTY(double elapsedSeconds READ elapsedSeconds NOTIFY elapsedChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(QString lastFile READ lastFile NOTIFY lastFileChanged)
    Q_PROPERTY(QString warning READ warning NOTIFY warningChanged)
public:
    explicit Engine(RecorderBackend *backend, QString recordingDir, QObject *parent = nullptr);
    QString state() const;
    bool active() const { return m_state != Idle; }
    double elapsedSeconds() const;
    QString elapsedText() const;
    QString message() const { return m_message; }
    QString lastFile() const { return m_lastFile; }
    QString warning() const { return m_warning; }
    // True when a still from the file is (nearly) pure black. Static so tests and other code can reuse it.
    static bool looksBlack(const QByteArray &gray32x18);

    Q_INVOKABLE void start(int pick);     // 1 my screen is wired; 2 and 3 say so instead of pretending
    Q_INVOKABLE void toggle(int pick);    // start when idle, stop when recording (the Alt+Shift+V behaviour)
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void stop();
    static QString formatElapsed(double seconds);   // 00:12:34
signals:
    void stateChanged();
    void elapsedChanged();
    void messageChanged();
    void lastFileChanged();
    void warningChanged();
    void checkedFile(const QString &path, bool black);   // after the saved file was inspected
    void saved(const QString &path);
private:
    enum State { Idle, Starting, Recording, Paused, Stopping };
    void setState(State s);
    void setMessage(const QString &m);
    void onStarted();
    void onFailed(const QString &why);
    void onFinished(const QString &path);
    void checkPicture(const QString &path);
    RecorderBackend *m_backend;
    QString m_dir, m_message, m_lastFile, m_warning;
    State m_state = Idle;
    QElapsedTimer m_clock;
    qint64 m_banked = 0;   // ms recorded before the current running stretch
    QTimer m_tick;
};
