#include "engine.h"
#include <QDateTime>
#include "review.h"
#include "mobslog.h"
#include <QUrl>
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
    connect(backend, &RecorderBackend::streamState, this, &Engine::onStreamState);
    connect(backend, &RecorderBackend::streamStats, this, [this](int kbps, int dropped) {
        if (!m_takeStreams || (kbps == m_streamKbps && dropped == m_streamDropped)) return;
        static qint64 lastLogged = 0; const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - lastLogged > 10000 || dropped != m_streamDropped) { lastLogged = now; MobsLog::write("stream", QString("sending %1 kbps, %2 frames dropped").arg(kbps).arg(dropped)); }
        m_streamKbps = kbps; m_streamDropped = dropped; emit streamStatsChanged();
    });
    m_armTimer.setSingleShot(true);
    connect(&m_armTimer, &QTimer::timeout, this, [this] {
        if (!m_streamArmed || m_state != Idle) return;
        setStreamArmed(false);
        setMessage("go live was switched off: it was armed for " + QString::number(m_armTimeoutMs / 60000) + " minutes and no take started");
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
    const auto live = [](State x) { return x == Recording || x == Paused; };
    const bool livenessChanged = live(s) != live(m_state);
    const QString from = state();
    m_state = s;
    MobsLog::write("engine", QString("state %1 -> %2").arg(from, state()));
    emit stateChanged();
    emit elapsedChanged();
    // Only on a CHANGE, so the backend is not nagged on every pause/resume. Never while starting: the scene may still be
    // waiting for the screen picker, and a second build of it must not race the recording's own.
    if (livenessChanged) applyPreview();
    if (s == Idle) endBroadcast();
}
void Engine::setMessage(const QString &m) {
    if (m == m_message) return;
    m_message = m;
    if (!m.isEmpty()) MobsLog::write("engine", "message: " + m);
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
    if (pick < 1 || pick > 4) return;
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
    StreamSpec stream;
    if (m_streamArmed) {
        stream = m_streamFn ? m_streamFn() : StreamSpec{};
        if (stream.url.isEmpty() || stream.key.isEmpty()) {
            setMessage("go live needs the server address and stream key: set them in EXPERT > live");
            emit liveSetupNeeded();
            return;
        }
        stream.on = true;
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
    spec.stream = stream;
    m_takeStreams = stream.on;
    m_streamState = stream.on ? "connecting" : "off"; m_streamProblem.clear(); m_streamKbps = 0; m_streamDropped = 0;
    m_armTimer.stop();
    m_streamArmed = false;   // armed means "the next take"; this IS that take
    if (stream.on) { emit streamChanged(); emit streamStatsChanged(); }
    m_currentAudio = spec.audio;
    m_currentFolder = dir;
    m_currentMic = spec.mic;
    m_recPick = pick;
    MobsLog::write("engine", QString("start pick=%1 camera=%2 hardware=%3 crf=%4 mic=%5 desktop=%6 live=%7 folder=%8")
        .arg(pick).arg(cam.valid() ? cam.path : "none").arg(spec.hardware).arg(spec.crf).arg(spec.mic).arg(spec.audio.desktop)
        .arg(stream.on ? QString("yes(%1 %2 kbps %3p)").arg(QUrl(stream.url).host()).arg(stream.kbps).arg(stream.height) : "no", dir));
    setState(Starting);
    m_backend->begin(spec);
}

void Engine::toggle(int pick) {
    if (m_state == Idle) start(pick);
    else stop();
}

void Engine::togglePause() {
    // A live picture cannot be paused: viewers would keep watching while the saved file quietly skipped the same stretch.
    if (broadcasting()) { setMessage("you are live: pause is off while you broadcast"); return; }
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

void Engine::forgetScreen() {
    if (active()) return;
    MobsLog::write("engine", "change screen: forgetting the chosen screen and guest window");
    m_backend->forgetCapture();
}

void Engine::onFailed(const QString &why) {
    MobsLog::write("engine", "failed: " + why);
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
    MobsLog::write("engine", path.isEmpty() ? QString("finished: cancelled, nothing recorded") : "finished: saved " + path);
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
    m_previewWish = wanted;
    m_previewPick = pick;
    applyPreview();
}

// The preview follows the ready screen's choice, plus a take in progress (recording or paused) always shows itself.
void Engine::applyPreview() {
    const bool live = m_state == Recording || m_state == Paused;
    const int pick = live ? m_recPick : m_previewPick;
    const CameraSpec cam = m_camera ? m_camera() : CameraSpec{};
    const bool usable = pick >= 1 && pick <= 4 && (pick == 1 || cam.valid());
    const bool on = (m_previewWish || live) && usable;
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

// ---------------------------------------------------------------- going live

void Engine::setStreamArmed(bool armed) {
    if (m_state != Idle) armed = false;   // a take in progress is what it is; arming is for the next one
    if (armed == m_streamArmed) return;
    MobsLog::write("stream", armed ? "armed to go live" : "disarmed");
    m_streamArmed = armed;
    if (armed) m_armTimer.start(m_armTimeoutMs); else m_armTimer.stop();
    emit streamChanged();
}

void Engine::onStreamState(const QString &state, const QString &detail) {
    if (!m_takeStreams) return;   // a late word from a broadcast that is already over
    const QString problem = (state == "failed" || state == "reconnecting") ? detail : QString();
    if (state == m_streamState && problem == m_streamProblem) return;
    MobsLog::write("stream", QString("state %1 -> %2 %3").arg(m_streamState, state, detail));
    m_streamState = state; m_streamProblem = problem;
    if (state != "live") { m_streamKbps = 0; emit streamStatsChanged(); }
    emit streamChanged();
}

// The take is over: whatever happened to the broadcast, the next take starts from "not live" and un-armed.
void Engine::endBroadcast() {
    const bool changed = m_streamArmed || m_takeStreams || m_streamState != "off";
    m_streamArmed = false; m_takeStreams = false; m_streamState = "off"; m_streamProblem.clear();
    m_streamKbps = 0; m_streamDropped = 0;
    m_armTimer.stop();
    if (changed) { emit streamChanged(); emit streamStatsChanged(); }
}
