#pragma once
#include "recorderbackend.h"
#include <QElapsedTimer>
#include <QProcess>
#include <QObject>
#include <QTimer>
#include <functional>
#include <utility>

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
    Q_PROPERTY(bool previewing READ previewing NOTIFY previewingChanged)
public:
    explicit Engine(RecorderBackend *backend, QString recordingDir, QObject *parent = nullptr);
    QString state() const;
    bool active() const { return m_state != Idle; }
    double elapsedSeconds() const;
    QString elapsedText() const;
    QString message() const { return m_message; }
    QString lastFile() const { return m_lastFile; }
    QString warning() const { return m_warning; }
    bool previewing() const { return m_previewing; }
    // True when a still from the file is (nearly) pure black. Static so tests and other code can reuse it.
    static bool looksBlack(const QByteArray &gray32x18);

    Q_INVOKABLE void start(int pick);     // 2 and 3 need a chosen camera: without one they say so instead of recording nothing
    void setCameraProvider(std::function<CameraSpec()> provider) { m_camera = std::move(provider); }
    // Where recordings go, and how, asked at every start so a change in the Pro drawer applies to the next recording.
    void setFolderProvider(std::function<QString()> provider) { m_folderFn = std::move(provider); }
    void setOptionsProvider(std::function<std::pair<int, bool>()> provider) { m_optionsFn = std::move(provider); }   // {crf, mic}
    // Show what would be recorded. wanted=false, or a camera mode with no usable camera, switches the preview off.
    // Safe to call again whenever the pick, camera or placement changes. Never touches the recording state.
    Q_INVOKABLE void setPreview(bool wanted, int pick);
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
    void previewingChanged();
    void checkedFile(const QString &path, bool black);   // after the saved file was inspected
    void saved(const QString &path);
    void cameraNeeded();                   // the user asked for a camera recording but has not chosen a camera
private:
    enum State { Idle, Starting, Recording, Paused, Stopping };
    void setState(State s);
    void setMessage(const QString &m);
    void onStarted();
    void onFailed(const QString &why);
    void onFinished(const QString &path);
    void checkPicture(const QString &path);
    RecorderBackend *m_backend;
    std::function<CameraSpec()> m_camera;
    std::function<QString()> m_folderFn;
    std::function<std::pair<int, bool>()> m_optionsFn;
    QString m_dir, m_message, m_lastFile, m_warning;
    QList<double> m_pauseMarks;   // where in the take (seconds of recorded time) the user paused
    State m_state = Idle;
    bool m_previewing = false;
    QElapsedTimer m_clock;
    qint64 m_banked = 0;   // ms recorded before the current running stretch
    QTimer m_tick;
};
