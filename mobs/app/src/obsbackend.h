#pragma once
#include "recorderbackend.h"
#include "streamhealth.h"
#include <QElapsedTimer>
#include <QImage>
#include <QMutex>
#include <QTimer>
#include <atomic>

struct obs_scene;
struct obs_source;
struct obs_output;
struct obs_encoder;
struct obs_service;
struct obs_scene_item;
struct video_data;
struct calldata;
struct obs_volmeter;

// Records with libobs running INSIDE this process (ADR 0006): no OBS application, nothing hidden.
// Rules proven in spike 4: platform before obs_startup, engine plugins by name, destroy queue before shutdown.
//
// Two lifetimes, kept apart so a preview and a recording can share one scene:
//   the GRAPH     (scene + screen/camera sources): lives while previewing OR recording
//   the RECORDING (microphone, encoders, file output): lives only while recording
class ObsBackend : public RecorderBackend {
    Q_OBJECT
public:
    // tokenFile keeps the screen-share restore token between runs, so only the first screen use opens the picker.
    explicit ObsBackend(QString tokenFile, QObject *parent = nullptr);
    ~ObsBackend() override;
    void begin(const RecordSpec &spec) override;
    void pause(bool paused) override;
    void end() override;
    void setPreview(bool on, const RecordSpec &spec) override;
    void forgetCapture() override;
    void setMicCheck(bool on, const AudioSpec &audio) override;
    void checkHardware() override;
private:
    enum Phase { Off, Waiting, Running, Stopping };
    bool ensureStarted(QString *error);
    bool ensureGraph(const RecordSpec &spec, QString *error);
    void applyPlacement(const RecordSpec &spec);
    QString screenTokenFile() const;
    void tickGraph();
    void dropGraph();
    bool startOutput(const RecordSpec &spec, QString *error);
    void stopOutput();
    bool startStream(const RecordSpec &spec, QString *error);
    void stopStream();
    void pollStream();
    QString redacted(QString text) const;
    static void streamStarted(void *self, calldata *data);
    static void streamReconnecting(void *self, calldata *data);
    static void streamReconnected(void *self, calldata *data);
    static void streamStopped(void *self, calldata *data);
    bool startMic(const AudioSpec &audio, QString *error);
    void stopAudio();
    void pollAudio();
    static void micLevel(void *, const float *, const float *, const float *);
    static void desktopLevel(void *, const float *, const float *, const float *);
    bool sourcesReady() const;
    void poll();
    void fail(const QString &why);
    void finishAttempt();
    void addPreviewCallback();
    void removePreviewCallback();
    void onRawFrame(video_data *frame);
    static void outputStopped(void *self, calldata *data);
    std::atomic<int> m_stopCode{1}; // 1 means no stop signal yet (libobs stop codes are <= 0)
    static void rawFrameCb(void *self, video_data *frame);

    QString m_tokenFile, m_path;
    RecordSpec m_spec;                      // the recording being made (quality, microphone)
    bool m_libobsUp = false, m_needScreen = true, m_needCamera = false, m_testScreen = false, m_testCamera = false;
    bool m_graphUp = false, m_previewOn = false, m_previewCb = false;
    bool m_loggedScreen = false, m_loggedCamera = false;
    QTimer m_graphTick;
    int m_graphPick = 0;
    bool m_graphPip = false;
    QString m_graphCamera;
    RecordSpec m_previewSpec;               // the last picture asked for, so "change screen" can rebuild it
    Phase m_phase = Off;
    QTimer m_poll, m_audioTick;
    obs_source *m_desktop = nullptr;
    obs_volmeter *m_micMeter = nullptr, *m_desktopMeter = nullptr;
    std::atomic<float> m_micDb{-60}, m_desktopDb{-60};
    std::atomic<qint64> m_micFrameAt{0}, m_desktopFrameAt{0};
    qint64 m_audioStarted = 0;
    QElapsedTimer m_waited, m_screenReady;
    obs_scene *m_scene = nullptr;
    obs_scene_item *m_item = nullptr, *m_camItem = nullptr;
    obs_source *m_video = nullptr, *m_camera = nullptr, *m_mic = nullptr;
    obs_encoder *m_venc = nullptr, *m_aenc = nullptr;
    obs_output *m_out = nullptr;
    // The broadcast, when the take has one: its own constant-rate encoders and RTMP output, beside the file's. libobs reports
    // its progress from its own threads into these atomics; pollStream() turns them into signals on the Qt thread.
    obs_encoder *m_svenc = nullptr, *m_saenc = nullptr;
    obs_output *m_stream = nullptr;
    obs_service *m_service = nullptr;
    QString m_streamKey;                       // kept only to scrub it out of any error text
    QString m_streamShown;                     // the last state sent up, so a state is announced once
    std::atomic<int> m_streamStop{1};          // 1 = running; otherwise libobs's stop code
    std::atomic<bool> m_streamUp{false}, m_streamRetrying{false};
    bool m_streamAsked = false;                // end() asked it to stop: a stop is then an ending, not a failure
    QElapsedTimer m_streamClock, m_streamTime;   // the first restarts every second for the bitrate; the second runs from the start of the broadcast
    StreamHealth m_health;                       // quick drops in a row = X refusing the stream
    QString m_streamAdvice;                      // what to tell the person when MOBS stops retrying
    bool m_streamGaveUp = false;
    quint64 m_streamBytes = 0;
    // newest preview frame, handed from libobs's video thread to the Qt main thread
    QMutex m_frameMutex;
    QImage m_latest;
    std::atomic<bool> m_frameQueued{false};
};
