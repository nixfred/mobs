#include "engine.h"
#include <QDateTime>
#include <QDir>

Engine::Engine(RecorderBackend *backend, QString recordingDir, QObject *parent)
    : QObject(parent), m_backend(backend), m_dir(std::move(recordingDir)) {
    m_tick.setInterval(250);
    connect(&m_tick, &QTimer::timeout, this, &Engine::elapsedChanged);
    connect(backend, &RecorderBackend::waitingForUser, this, [this](const QString &w) { setMessage(w); });
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

void Engine::start(int pick) {
    if (m_state != Idle) return;
    if (pick != 1) {
        setMessage("camera sources are not wired yet: pick \"my screen\" to record");
        return;
    }
    QDir().mkpath(m_dir);
    const QString path = m_dir + "/mobs " + QDateTime::currentDateTime().toString("yyyy-MM-dd HH-mm-ss") + ".mp4";
    m_banked = 0;
    m_clock.invalidate();
    setMessage({});
    setState(Starting);
    m_backend->begin(pick, path);
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
    m_tick.stop();
    m_clock.invalidate();
    setMessage(why);
    setState(Idle);
}

void Engine::onFinished(const QString &path) {
    m_tick.stop();
    m_clock.invalidate();
    if (path.isEmpty()) {
        setMessage("cancelled: nothing was recorded");
    } else {
        m_lastFile = path;
        emit lastFileChanged();
        setMessage("saved " + path);
        emit saved(path);
    }
    setState(Idle);
}
