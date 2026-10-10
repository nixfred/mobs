#include "pipcontroller.h"
#include "appstate.h"
#include "engine.h"
#include "mobslog.h"
#include "settings.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMediaDevices>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <algorithm>

PipController::PipController(AppState *state, Engine *engine, Settings *prefs, QSettings *store, QObject *parent)
    : QObject(parent), m_state(state), m_engine(engine), m_prefs(prefs), m_store(store) {
    m_placeTimer.setInterval(150);
    connect(&m_placeTimer, &QTimer::timeout, this, &PipController::tryPlace);
    connect(m_engine, &Engine::previewingChanged, this, [this] { rememberActive(); emit changed(); });
    connect(m_state, &AppState::pickChanged, this, &PipController::changed);
    connect(m_state, &AppState::screenChanged, this, &PipController::changed);
    connect(m_state, &AppState::cameraChanged, this, &PipController::changed);
    connect(m_prefs, &Settings::changed, this, &PipController::changed);
}

// Wanted whenever "screen + me" is showing its picture (ready with the preview on, armed, or a take in progress): Engine::previewing is exactly that.
bool PipController::wanted() const { return m_prefs->pip() && m_state->pick() == 2 && m_state->hasCamera() && m_engine->previewing(); }

bool PipController::testPattern() const {
    return !qEnvironmentVariable("MOBS_TEST_CAMERA").isEmpty() || m_state->camera().startsWith("fake:");
}

QCameraDevice PipController::device() const {
    const QString path = m_state->camera();
    if (path.isEmpty() || testPattern()) return {};
    const QString node = QFileInfo(path).canonicalFilePath();     // /dev/v4l/by-id/...-index0 -> /dev/video0
    const auto inputs = QMediaDevices::videoInputs();
    for (const QCameraDevice &d : inputs) if (!node.isEmpty() && QString::fromUtf8(d.id()) == node) return d;
    for (const QCameraDevice &d : inputs) if (d.description() == m_state->cameraName()) return d;
    return {};
}

// 1280x720 (MJPEG when the camera has it, for the frame rate), else the smallest format that is still at least 640x360.
QCameraFormat PipController::format() const {
    const QCameraDevice d = device();
    QCameraFormat best; double bestScore = -1;
    for (const QCameraFormat &f : d.videoFormats()) {
        const QSize r = f.resolution();
        double score = 0;
        if (r.width() == 1280 && r.height() == 720) score += 100;
        else if (r.width() >= 640 && r.height() >= 360) score += 50 - (r.width() * r.height()) / 100000.0;
        if (f.maxFrameRate() >= 24) score += 10;
        if (f.pixelFormat() == QVideoFrameFormat::Format_Jpeg) score += 5;
        if (score > bestScore) { bestScore = score; best = f; }
    }
    return best;
}

// ---------------------------------------------------------------- Hyprland: float, pin, place

bool PipController::onHyprland() const {
    if (!QGuiApplication::platformName().contains("wayland")) return false;
    if (QStandardPaths::findExecutable("hyprctl").isEmpty()) return false;
    return !qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE").isEmpty() || QDir(qEnvironmentVariable("XDG_RUNTIME_DIR") + "/hypr").exists();
}

static QProcessEnvironment hyprEnvironment() {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (env.value("HYPRLAND_INSTANCE_SIGNATURE").isEmpty()) {     // started by the hotkey's D-Bus launch: no compositor variables, so find the session
        const QDir runtime(env.value("XDG_RUNTIME_DIR") + "/hypr");
        const auto dirs = runtime.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
        if (!dirs.isEmpty()) env.insert("HYPRLAND_INSTANCE_SIGNATURE", dirs.first().fileName());
    }
    return env;
}

QJsonDocument PipController::hyprJson(const QStringList &args) const {
    QProcess p; p.setProcessEnvironment(hyprEnvironment());
    p.start(QStandardPaths::findExecutable("hyprctl"), args);
    if (!p.waitForFinished(2000) || p.exitCode() != 0) return {};
    return QJsonDocument::fromJson(p.readAllStandardOutput());
}

bool PipController::hyprEval(const QString &lua) const {
    QProcess p; p.setProcessEnvironment(hyprEnvironment());
    p.start(QStandardPaths::findExecutable("hyprctl"), {"eval", lua});
    if (!p.waitForFinished(2000)) return false;
    const QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    if (p.exitCode() != 0 || (!out.isEmpty() && out != "ok")) MobsLog::write("pip", "hyprctl eval said: " + out + " for " + lua.left(120));
    return p.exitCode() == 0;
}

void PipController::rememberActive() {
    if (!onHyprland() || !wanted() || m_shown) return;
    const QString addr = hyprJson({"activewindow", "-j"}).object().value("address").toString();
    if (!addr.isEmpty()) m_prevActive = addr;
}

void PipController::windowShown() {
    m_shown = true; m_tries = 0;
    if (!onHyprland()) { MobsLog::write("pip", "webcam window shown (not on Hyprland: it is an ordinary window, move it yourself)"); return; }
    m_placeTimer.start();
}

void PipController::tryPlace() {
    if (!m_shown) { m_placeTimer.stop(); return; }
    const QJsonArray clients = hyprJson({"clients", "-j"}).array();
    const QString address = addressOf(clients, kTitle, QCoreApplication::applicationPid());
    if (address.isEmpty()) {
        if (++m_tries > 40) { m_placeTimer.stop(); MobsLog::write("pip", "gave up waiting for the compositor to list the webcam window"); }
        return;
    }
    m_placeTimer.stop();
    place(address, clients);
}

void PipController::place(const QString &address, const QJsonArray &clients) {
    const QJsonObject client = clientAt(clients, address);
    const QJsonArray monitors = hyprJson({"monitors", "-j"}).array();
    const QString win = QString("window=\"address:%1\"").arg(address);
    const auto dsp = [&](const QString &call) { return hyprEval("hl.dispatch(hl.dsp." + call + ")"); };

    // Where: where it was left last time (if that monitor is still there), else the top-right of MOBS's own monitor.
    QJsonObject monitor = monitorByName(monitors, m_store->value("pipMonitor").toString());
    QSize size(qBound(160, m_store->value("pipW", 480).toInt(), 1920), qBound(90, m_store->value("pipH", 270).toInt(), 1080));
    QRect rect;
    if (!monitor.isEmpty() && m_store->contains("pipX")) rect = fitInside(QRect(QPoint(m_store->value("pipX").toInt(), m_store->value("pipY").toInt()), size), monitor);
    if (rect.isNull()) {
        for (const QJsonValue &v : clients) {            // MOBS's own main window decides the monitor
            const QJsonObject c = v.toObject();
            if (c.value("pid").toInt() == QCoreApplication::applicationPid() && c.value("title").toString() != kTitle) { monitor = monitorById(monitors, c.value("monitor").toInt()); break; }
        }
        if (monitor.isEmpty()) for (const QJsonValue &v : monitors) if (v.toObject().value("focused").toBool()) monitor = v.toObject();
        if (monitor.isEmpty() && !monitors.isEmpty()) monitor = monitors.first().toObject();
        if (!monitor.isEmpty()) rect = defaultRect(monitor, size);
    }

    if (!client.value("floating").toBool()) dsp(QString("window.float({action=\"set\",%1})").arg(win));
    if (!client.value("pinned").toBool()) dsp(QString("window.pin({%1})").arg(win));         // on every workspace: changing workspaces must not lose you from the stream
    if (!rect.isNull()) {
        dsp(QString("window.resize({x=%1,y=%2,%3})").arg(rect.width()).arg(rect.height()).arg(win));
        dsp(QString("window.move({x=%1,y=%2,%3})").arg(rect.x()).arg(rect.y()).arg(win));
    }
    dsp(QString("window.alter_zorder({mode=\"top\",%1})").arg(win));
    // A new window takes the keyboard. Give it straight back: the person is typing in something else.
    if (!m_prevActive.isEmpty() && m_prevActive != address && !clientAt(clients, m_prevActive).isEmpty()) dsp(QString("focus({window=\"address:%1\"})").arg(m_prevActive));
    MobsLog::write("pip", QString("webcam window %1 placed at %2,%3 %4x%5 on %6 (floating, pinned to every workspace); SUPER + drag moves it")
        .arg(address).arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()).arg(monitor.value("name").toString()));
}

void PipController::windowHidden() {
    m_shown = false; m_placeTimer.stop();
    if (!onHyprland()) return;
    const QJsonArray clients = hyprJson({"clients", "-j"}).array();
    const QJsonObject c = clientAt(clients, addressOf(clients, kTitle, QCoreApplication::applicationPid()));
    if (c.isEmpty()) return;
    const QJsonArray at = c.value("at").toArray(), size = c.value("size").toArray();
    const QJsonObject monitor = monitorById(hyprJson({"monitors", "-j"}).array(), c.value("monitor").toInt());
    if (at.size() < 2 || size.size() < 2 || monitor.isEmpty()) return;
    m_store->setValue("pipX", at[0].toInt()); m_store->setValue("pipY", at[1].toInt());
    m_store->setValue("pipW", size[0].toInt()); m_store->setValue("pipH", size[1].toInt());
    m_store->setValue("pipMonitor", monitor.value("name").toString()); m_store->sync();
    MobsLog::write("pip", QString("webcam window left at %1,%2 %3x%4 on %5").arg(at[0].toInt()).arg(at[1].toInt()).arg(size[0].toInt()).arg(size[1].toInt()).arg(monitor.value("name").toString()));
    m_prevActive.clear();
}

// ---------------------------------------------------------------- pure helpers

QString PipController::addressOf(const QJsonArray &clients, const QString &title, qint64 pid) {
    for (const QJsonValue &v : clients) {
        const QJsonObject c = v.toObject();
        if (c.value("title").toString() == title && c.value("pid").toInteger() == pid) return c.value("address").toString();   // title AND pid: never someone else's window
    }
    return {};
}

QJsonObject PipController::clientAt(const QJsonArray &clients, const QString &address) {
    if (address.isEmpty()) return {};
    for (const QJsonValue &v : clients) if (v.toObject().value("address").toString() == address) return v.toObject();
    return {};
}

QJsonObject PipController::monitorById(const QJsonArray &monitors, int id) {
    for (const QJsonValue &v : monitors) if (v.toObject().value("id").toInt(-1) == id) return v.toObject();
    return {};
}

QJsonObject PipController::monitorByName(const QJsonArray &monitors, const QString &name) {
    if (name.isEmpty()) return {};
    for (const QJsonValue &v : monitors) if (v.toObject().value("name").toString() == name) return v.toObject();
    return {};
}

// Hyprland's monitor is in pixels and `scale` turns it into layout units; `reserved` is the bar and gaps (left, top, right, bottom).
static QRect usableArea(const QJsonObject &m) {
    const double scale = std::max(0.1, m.value("scale").toDouble(1.0));
    const QJsonArray r = m.value("reserved").toArray();
    const int left = r.size() > 0 ? r[0].toInt() : 0, top = r.size() > 1 ? r[1].toInt() : 0, right = r.size() > 2 ? r[2].toInt() : 0, bottom = r.size() > 3 ? r[3].toInt() : 0;
    const int w = int(m.value("width").toDouble() / scale), h = int(m.value("height").toDouble() / scale);
    return QRect(m.value("x").toInt() + left, m.value("y").toInt() + top, w - left - right, h - top - bottom);
}

QRect PipController::defaultRect(const QJsonObject &monitor, const QSize &size, int margin) {
    const QRect a = usableArea(monitor);
    return QRect(a.right() + 1 - margin - size.width(), a.top() + margin, size.width(), size.height());
}

QRect PipController::fitInside(const QRect &want, const QJsonObject &monitor) {
    const QRect a = usableArea(monitor);
    if (a.isEmpty() || !a.intersects(want)) return {};
    QRect r = want;
    r.moveLeft(qBound(a.left(), r.left(), std::max(a.left(), a.right() + 1 - r.width())));
    r.moveTop(qBound(a.top(), r.top(), std::max(a.top(), a.bottom() + 1 - r.height())));
    return r;
}
