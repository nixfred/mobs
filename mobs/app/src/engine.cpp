#include "engine.h"
#include <QDateTime>
#include "review.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

Engine::Engine(RecorderBackend *backend, QString recordingDir, QObject *parent)
    : QObject(parent), m_backend(backend), m_dir(std::move(recordingDir)) {
    m_tick.setInterval(250);
    connect(&m_tick, &QTimer::timeout, this, &Engine::elapsedChanged);
    connect(backend, &RecorderBackend::waitingForUser, this, [this](const QString &w) { setMessage(w); });
    connect(backend, &RecorderBackend::previewFailed, this, [this](const QString &why) {
        if (m_previewing) { m_previewing = false; emit previewingChanged(); }
        setMessage(why);
    });
    connect(backend, &RecorderBackend::audioLevels, this, [this](double micDb, double desktopDb, bool micFrames, bool desktopFrames, const QString &problem) {
        m_micDb = std::isfinite(micDb) ? std::clamp(micDb, -60.0, 12.0) : -60;
        m_desktopDb = std::isfinite(desktopDb) ? std::clamp(desktopDb, -60.0, 12.0) : -60;
        m_micFrames = micFrames; m_desktopFrames = desktopFrames; m_audioProblem = problem; emit audioChanged();
    });
    connect(backend, &RecorderBackend::micCheckFailed, this, [this](const QString &why) {
        m_micChecking = false; m_audioProblem = why; emit audioChanged();
    });
    connect(backend, &RecorderBackend::hardwareChecked, this, [this](bool nvenc, const QString &message) {
        m_hardwareKnown = true; m_nvencAvailable = nvenc; m_hardwareMessage = message; emit hardwareChanged();
    });
    connect(backend, &RecorderBackend::started, this, &Engine::onStarted);
    connect(backend, &RecorderBackend::failed, this, &Engine::onFailed);
    connect(backend, &RecorderBackend::finished, this, &Engine::onFinished);
}

QString Engine::state() const {
    switch (m_state) {
    case Idle: return "idle";
    case Starting: return "starting";
    case Recording: return "recording";
    case Paused: return "paused";
    case Stopping: return "stopping";
    }
    return "idle";
}

double Engine::elapsedSeconds() const {
    const qint64 running = (m_state == Recording && m_clock.isValid()) ? m_clock.elapsed() : 0;
    return (m_banked + running) / 1000.0;
}

QString Engine::formatElapsed(double seconds) {
    const int s = int(seconds);
    return QString("%1:%2:%3").arg(s / 3600, 2, 10, QChar('0')).arg(s / 60 % 60, 2, 10, QChar('0')).arg(s % 60, 2, 10, QChar('0'));
}
QString Engine::elapsedText() const { return formatElapsed(elapsedSeconds()); }

void Engine::setState(State s) {
    if (s == m_state) return;
    m_state = s;
    emit stateChanged();
    emit elapsedChanged();
}
void Engine::setMessage(const QString &m) {
    if (m == m_message) return;
    m_message = m;
    emit messageChanged();
}

QString Engine::recordingPathFor(const QString &dir, const QString &stamp) {
    const QString base = QDir(dir).filePath("mobs " + stamp);
    QString path = base + ".mp4";
    for (int n = 2; QFileInfo::exists(path) || QFileInfo(path).isSymLink() || QFileInfo::exists(Review::sidecarPath(path)) || QFileInfo(Review::sidecarPath(path)).isSymLink(); ++n)
        path = base + " " + QString::number(n) + ".mp4";
    return path;
}

void Engine::start(int pick) {
    if (m_state != Idle) return;
    if (pick < 1 || pick > 3) return;
    if (m_startGuard) {
        const QString problem = m_startGuard();
        if (!problem.isEmpty()) { setMessage(problem); return; }
    }
    const CameraSpec cam = m_camera ? m_camera() : CameraSpec{};
    if (pick != 1 && !cam.valid()) {
        setMessage("choose a camera first: click the camera chip");
        emit cameraNeeded();
        return;
    }
    const QString dir = m_folderFn ? m_folderFn() : m_dir;
    if (!QDir().mkpath(dir) || !QFileInfo(dir).isWritable()) {
        setMessage("cannot save to " + dir + ": choose another folder in EXPERT > recording");
        return;
    }
    const QString path = recordingPathFor(dir, QDateTime::currentDateTime().toString("yyyy-MM-dd HH-mm-ss-zzz"));
    checkMic(false);
    m_banked = 0;
    m_clock.invalidate();
    m_pauseMarks.clear();
    setMessage({});
    RecordSpec spec{pick, path, cam};
    if (m_optionsFn) { const auto o = m_optionsFn(); spec.crf = o.first; spec.mic = o.second; }
    if (m_presetFn) spec.encoderPreset = m_presetFn();
    if (m_audioFn) spec.audio = m_audioFn();
    if (m_hardwareFn) spec.hardware = m_hardwareFn();
    m_currentAudio = spec.audio;
    m_currentFolder = dir;
    m_currentMic = spec.mic;
    setState(Starting);
    m_backend->begin(spec);
}

void Engine::toggle(int pick) {
    if (m_state == Idle) start(pick);
    else stop();
}

void Engine::togglePause() {
    if (m_state == Recording) {
        m_banked += m_clock.elapsed();
        m_clock.invalidate();
        m_tick.stop();
        m_pauseMarks.append(m_banked / 1000.0);
        setState(Paused);
        m_backend->pause(true);
    } else if (m_state == Paused) {
        m_clock.start();
        m_tick.start();
        setState(Recording);
        m_backend->pause(false);
    }
}

void Engine::stop() {
    if (m_state == Idle || m_state == Stopping) return;
    if (m_state == Recording) m_banked += m_clock.elapsed();
    m_clock.invalidate();
    m_tick.stop();
    setState(Stopping);
    m_backend->end();
}

void Engine::onStarted() {
    if (m_state != Starting) return;
    m_clock.start();
    m_tick.start();
    setMessage({});
    setState(Recording);
}

void Engine::onFailed(const QString &why) {
    if (m_state == Recording && m_clock.isValid()) m_banked += m_clock.elapsed();
    m_tick.stop();
    m_clock.invalidate();
    setMessage(why);
    setState(Idle);
}

void Engine::onFinished(const QString &path) {
    if (m_state == Recording && m_clock.isValid()) m_banked += m_clock.elapsed();
    m_tick.stop();
    m_clock.invalidate();
    if (path.isEmpty()) {
        setMessage("cancelled: nothing was recorded");
    } else {
        m_lastFile = path;
        emit lastFileChanged();
        setMessage("saved " + path);
        if (!m_warning.isEmpty()) { m_warning.clear(); emit warningChanged(); }
        // The pause points go beside the take so the editor can show them (they are positions in the FINAL file: pauses are cut out).
        if (!m_pauseMarks.isEmpty() && QFileInfo::exists(path)) Review::writePauses(path, m_pauseMarks);
        emit saved(path);
        checkPicture(path);
    }
    setState(Idle);
}

bool Engine::looksBlack(const QByteArray &g) {
    if (g.isEmpty()) return false;               // nothing to judge: never cry wolf
    unsigned char mx = 0;
    for (char c : g) mx = std::max(mx, static_cast<unsigned char>(c));   // QByteArray chars are SIGNED: 255 reads as -1
    return mx < 4;                                                        // every pixel is (almost) 0
}

// Some portal picks (a dragged REGION, on gus) record a valid file full of black frames. Say so instead of
// leaving the user to find out later. A still from 1 s in, shrunk to 32x18 grey, is plenty to tell black from a screen.
void Engine::checkPicture(const QString &path) {
    const QString ffmpeg = QStandardPaths::findExecutable("ffmpeg");
    if (ffmpeg.isEmpty() || !QFileInfo::exists(path)) return;
    auto *p = new QProcess(this);
    connect(p, &QProcess::errorOccurred, p, [p](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) p->deleteLater();
    });
    connect(p, &QProcess::finished, this, [this, p, path](int code, QProcess::ExitStatus st) {
        const QByteArray bytes = p->readAllStandardOutput();
        p->deleteLater();
        if (code != 0 || st != QProcess::NormalExit || bytes.size() < 32 * 18) return;
        const bool black = looksBlack(bytes.left(32 * 18));
        if (black && m_lastFile == path) {
            m_warning = "the picture in this recording is black. Choosing a region can record black here: pick a screen or a window.";
            emit warningChanged();
        }
        emit checkedFile(path, black);
    });
    p->start(ffmpeg, {"-nostdin", "-v", "error", "-ss", "1", "-i", path, "-frames:v", "1", "-vf", "scale=32:18,format=gray", "-f", "rawvideo", "-"});
}

void Engine::setPreview(bool wanted, int pick) {
    const CameraSpec cam = m_camera ? m_camera() : CameraSpec{};
    const bool usable = pick >= 1 && pick <= 3 && (pick == 1 || cam.valid());
    const bool on = wanted && usable;
    const bool wasOn = m_previewing;
    if (on != m_previewing) { m_previewing = on; emit previewingChanged(); }
    if (on || wasOn) m_backend->setPreview(on, RecordSpec{pick, QString(), cam});
}

void Engine::checkMic(bool on) {
    if (active()) return;
    if (on) {
        const QString problem = m_micGuard ? m_micGuard() : QString();
        if (!problem.isEmpty()) {
            if (m_micChecking) m_backend->setMicCheck(false, AudioSpec{});
            m_micChecking = false; m_audioProblem = problem; emit audioChanged(); return;
        }
    }
    if (!on && !m_micChecking) {
        if (!m_audioProblem.isEmpty()) { m_audioProblem.clear(); emit audioChanged(); }
        return;
    }
    m_micChecking = on; m_audioProblem.clear(); emit audioChanged();
    m_backend->setMicCheck(on, m_audioFn ? m_audioFn() : AudioSpec{});
}
