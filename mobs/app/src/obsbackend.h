#pragma once
#include "recorderbackend.h"
#include <QElapsedTimer>
#include <QTimer>
#include <memory>

struct obs_scene;
struct obs_source;
struct obs_output;
struct obs_encoder;
struct obs_scene_item;

// Records with libobs running INSIDE this process (ADR 0006): no OBS application, nothing hidden.
// Rules proven in spike 4: platform before obs_startup, engine plugins by name, destroy queue before shutdown.
class ObsBackend : public RecorderBackend {
    Q_OBJECT
public:
    // tokenFile keeps the screen-share restore token between runs, so only the first recording opens the picker.
    explicit ObsBackend(QString tokenFile, QObject *parent = nullptr);
    ~ObsBackend() override;
    void begin(const RecordSpec &spec) override;
    void pause(bool paused) override;
    void end() override;
private:
    enum Phase { Off, Waiting, Running, Stopping };
    bool ensureStarted(QString *error);
    bool buildGraph(const RecordSpec &spec, QString *error);
    bool sourcesReady() const;
    void poll();
    void fail(const QString &why);
    void teardown();
    QString m_tokenFile, m_path;
    bool m_libobsUp = false, m_needScreen = true, m_needCamera = false, m_testScreen = false, m_testCamera = false;
    Phase m_phase = Off;
    QTimer m_poll;
    QElapsedTimer m_waited, m_screenReady;
    obs_scene *m_scene = nullptr;
    obs_scene_item *m_item = nullptr, *m_camItem = nullptr;
    obs_source *m_video = nullptr, *m_camera = nullptr, *m_mic = nullptr;
    obs_encoder *m_venc = nullptr, *m_aenc = nullptr;
    obs_output *m_out = nullptr;
};
