#pragma once
#include "recorderbackend.h"
#include <QElapsedTimer>
#include <QImage>
#include <QMutex>
#include <QTimer>
#include <atomic>

struct obs_scene;
struct obs_source;
struct obs_output;
struct obs_encoder;
struct obs_scene_item;
struct video_data;

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
private:
    enum Phase { Off, Waiting, Running, Stopping };
    bool ensureStarted(QString *error);
    bool ensureGraph(const RecordSpec &spec, QString *error);
    void applyPlacement(const RecordSpec &spec);
    void dropGraph();
    bool startOutput(const QString &path, QString *error);
    void stopOutput();
    bool sourcesReady() const;
    void poll();
    void fail(const QString &why);
    void finishAttempt();
    void addPreviewCallback();
    void removePreviewCallback();
    void onRawFrame(video_data *frame);
    static void rawFrameCb(void *self, video_data *frame);

    QString m_tokenFile, m_path;
    bool m_libobsUp = false, m_needScreen = true, m_needCamera = false, m_testScreen = false, m_testCamera = false;
    bool m_graphUp = false, m_previewOn = false, m_previewCb = false;
    int m_graphPick = 0;
    QString m_graphCamera;
    Phase m_phase = Off;
    QTimer m_poll;
    QElapsedTimer m_waited, m_screenReady;
    obs_scene *m_scene = nullptr;
    obs_scene_item *m_item = nullptr, *m_camItem = nullptr;
    obs_source *m_video = nullptr, *m_camera = nullptr, *m_mic = nullptr;
    obs_encoder *m_venc = nullptr, *m_aenc = nullptr;
    obs_output *m_out = nullptr;
    // newest preview frame, handed from libobs's video thread to the Qt main thread
    QMutex m_frameMutex;
    QImage m_latest;
    std::atomic<bool> m_frameQueued{false};
};
