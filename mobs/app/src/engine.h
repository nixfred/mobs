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
    Q_PROPERTY(QString recordingFolder READ recordingFolder NOTIFY stateChanged)
    Q_PROPERTY(bool recordingMic READ recordingMic NOTIFY stateChanged)
    Q_PROPERTY(QString recordingMicDevice READ recordingMicDevice NOTIFY stateChanged)
    Q_PROPERTY(bool recordingDesktop READ recordingDesktop NOTIFY stateChanged)
    Q_PROPERTY(double micDb READ micDb NOTIFY audioChanged)
    Q_PROPERTY(double desktopDb READ desktopDb NOTIFY audioChanged)
    Q_PROPERTY(bool micFrames READ micFrames NOTIFY audioChanged)
    Q_PROPERTY(bool desktopFrames READ desktopFrames NOTIFY audioChanged)
    Q_PROPERTY(bool micChecking READ micChecking NOTIFY audioChanged)
    Q_PROPERTY(QString audioProblem READ audioProblem NOTIFY audioChanged)
    Q_PROPERTY(bool hardwareKnown READ hardwareKnown NOTIFY hardwareChanged)
    Q_PROPERTY(bool nvencAvailable READ nvencAvailable NOTIFY hardwareChanged)
    Q_PROPERTY(QString hardwareMessage READ hardwareMessage NOTIFY hardwareChanged)
    Q_PROPERTY(QString lastFile READ lastFile NOTIFY lastFileChanged)
    Q_PROPERTY(QString warning READ warning NOTIFY warningChanged)
    Q_PROPERTY(bool previewing READ previewing NOTIFY previewingChanged)
    // Going live. Armed means "the NEXT take is also broadcast"; it is never remembered, and it clears when that take ends.
    Q_PROPERTY(bool streamArmed READ streamArmed NOTIFY streamChanged)
    Q_PROPERTY(bool streamRequested READ streamRequested NOTIFY streamChanged)   // this take was started to broadcast
    Q_PROPERTY(bool broadcasting READ broadcasting NOTIFY streamChanged)         // connecting, live or reconnecting right now
    Q_PROPERTY(QString streamState READ streamState NOTIFY streamChanged)        // off, connecting, live, reconnecting, failed, ended
    Q_PROPERTY(QString streamProblem READ streamProblem NOTIFY streamChanged)
    Q_PROPERTY(int streamKbps READ streamKbps NOTIFY streamStatsChanged)
    Q_PROPERTY(int streamDropped READ streamDropped NOTIFY streamStatsChanged)
public:
    explicit Engine(RecorderBackend *backend, QString recordingDir, QObject *parent = nullptr);
    QString state() const;
    bool active() const { return m_state != Idle; }
    double elapsedSeconds() const;
    QString elapsedText() const;
    QString message() const { return m_message; }
    void clearIdleMessage() { if (!active()) setMessage({}); }
    void reportReviewFailure(const QString &message) { if (!active()) setMessage(message); }
    QString recordingFolder() const { return m_currentFolder; }
    bool recordingMic() const { return m_currentMic; }
    QString recordingMicDevice() const { return m_currentAudio.micDevice; }
    bool recordingDesktop() const { return m_currentAudio.desktop; }
    double micDb() const { return m_micDb; }
    double desktopDb() const { return m_desktopDb; }
    bool micFrames() const { return m_micFrames; }
    bool desktopFrames() const { return m_desktopFrames; }
    bool micChecking() const { return m_micChecking; }
    QString audioProblem() const { return m_audioProblem; }
    bool hardwareKnown() const { return m_hardwareKnown; }
    bool nvencAvailable() const { return m_nvencAvailable; }
    QString hardwareMessage() const { return m_hardwareMessage; }
    Q_INVOKABLE void checkMic(bool on);
    Q_INVOKABLE void checkHardware() { m_backend->checkHardware(); }
    Q_INVOKABLE void forgetScreen();   // "change screen": ask again which one to share
    void setAudioProvider(std::function<AudioSpec()> provider) { m_audioFn = std::move(provider); }
    void setHardwareProvider(std::function<bool()> provider) { m_hardwareFn = std::move(provider); }
    void setMicGuard(std::function<QString()> guard) { m_micGuard = std::move(guard); }
    QString lastFile() const { return m_lastFile; }
    QString warning() const { return m_warning; }
    bool previewing() const { return m_previewing; }
    bool streamArmed() const { return m_streamArmed; }
    bool streamRequested() const { return m_takeStreams; }
    bool broadcasting() const { return m_takeStreams && (m_streamState == "connecting" || m_streamState == "live" || m_streamState == "reconnecting"); }
    QString streamState() const { return m_streamState; }
    QString streamProblem() const { return m_streamProblem; }
    int streamKbps() const { return m_streamKbps; }
    int streamDropped() const { return m_streamDropped; }
    // Arming is a deliberate step before a take, and only then (never during one). Safe to call again; it never starts anything.
    Q_INVOKABLE void setStreamArmed(bool armed);
    void setStreamProvider(std::function<StreamSpec()> provider) { m_streamFn = std::move(provider); }
    void setArmTimeout(int ms) { m_armTimeoutMs = ms; }   // how long "go live" stays armed with no take started (tests shorten it)
    // True when a still from the file is (nearly) pure black. Static so tests and other code can reuse it.
    static bool looksBlack(const QByteArray &gray32x18);

    Q_INVOKABLE void start(int pick);     // 2 and 3 need a chosen camera: without one they say so instead of recording nothing
    void setStartGuard(std::function<QString()> guard) { m_startGuard = std::move(guard); }
    void setPresetProvider(std::function<QString()> provider) { m_presetFn = std::move(provider); }
    static QString recordingPathFor(const QString &dir, const QString &stamp);
    void setCameraProvider(std::function<CameraSpec()> provider) { m_camera = std::move(provider); }
    // Where recordings go, and how, asked at every start so a change in the Pro drawer applies to the next recording.
    void setFolderProvider(std::function<QString()> provider) { m_folderFn = std::move(provider); }
    void setOptionsProvider(std::function<std::pair<int, bool>()> provider) { m_optionsFn = std::move(provider); }   // {crf, mic}
    // Show what would be recorded. wanted=false, or a camera mode with no usable camera, switches the preview off.
    // A take that is recording or paused ALWAYS shows its preview, whatever was asked: a recorder you cannot see is one you cannot trust.
    // Safe to call again whenever the pick, camera or placement changes. Never touches the recording state.
    Q_INVOKABLE void setPreview(bool wanted, int pick);
    Q_INVOKABLE void toggle(int pick);    // start when idle, stop when recording (the Alt+Shift+V behaviour)
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void stop();
    // Only the explicit stalled-save confirmation may request immediate process exit.
    Q_INVOKABLE void requestForceQuit() { if (m_state == Stopping) emit forceQuitRequested(); }
    static QString formatElapsed(double seconds);   // 00:12:34
signals:
    void forceQuitRequested();
    void audioChanged();
    void hardwareChanged();
    void stateChanged();
    void elapsedChanged();
    void messageChanged();
    void lastFileChanged();
    void warningChanged();
    void previewingChanged();
    void checkedFile(const QString &path, bool black);   // after the saved file was inspected
    void saved(const QString &path);
    void cameraNeeded();                   // the user asked for a camera recording but has not chosen a camera
    void streamChanged();
    void streamStatsChanged();
    void liveSetupNeeded();                // go live was armed but the server address or stream key is missing
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
    std::function<QString()> m_startGuard, m_presetFn;
    std::function<QString()> m_folderFn;
    std::function<std::pair<int, bool>()> m_optionsFn;
    std::function<AudioSpec()> m_audioFn;
    std::function<bool()> m_hardwareFn;
    std::function<QString()> m_micGuard;
    AudioSpec m_currentAudio;
    bool m_micChecking = false, m_micFrames = false, m_desktopFrames = false;
    bool m_hardwareKnown = false, m_nvencAvailable = false;
    double m_micDb = -60, m_desktopDb = -60;
    QString m_audioProblem, m_hardwareMessage;
    bool m_currentMic = true;
    QString m_currentFolder;
    QString m_dir, m_message, m_lastFile, m_warning;
    QList<double> m_pauseMarks;   // where in the take (seconds of recorded time) the user paused
    State m_state = Idle;
    bool m_previewing = false;
    bool m_previewWish = false;                  // what the ready screen asked for
    int m_previewPick = 0, m_recPick = 0;        // the pick that wish was made for, and the pick of the take in progress
    void applyPreview();
    void onStreamState(const QString &state, const QString &detail);
    void endBroadcast();
    std::function<StreamSpec()> m_streamFn;
    QTimer m_armTimer;
    int m_armTimeoutMs = 15 * 60 * 1000;
    bool m_streamArmed = false, m_takeStreams = false;
    QString m_streamState = "off", m_streamProblem;
    int m_streamKbps = 0, m_streamDropped = 0;
    QElapsedTimer m_clock;
    qint64 m_banked = 0;   // ms recorded before the current running stretch
    QTimer m_tick;
};
