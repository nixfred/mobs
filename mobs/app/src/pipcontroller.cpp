#include "pipcontroller.h"
#include "appstate.h"
#include "chatmodel.h"
#include "engine.h"
#include "mobslog.h"
#include "notifier.h"
#include "settings.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMediaDevices>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <algorithm>

// Hyprland's monitor is in pixels and `scale` turns it into layout units; `reserved` is the bar and gaps (left, top, right, bottom).
static QRect usableArea(const QJsonObject &m) {
    const double scale = std::max(0.1, m.value("scale").toDouble(1.0));
    const QJsonArray r = m.value("reserved").toArray();
    const int left = r.size() > 0 ? r[0].toInt() : 0, top = r.size() > 1 ? r[1].toInt() : 0, right = r.size() > 2 ? r[2].toInt() : 0, bottom = r.size() > 3 ? r[3].toInt() : 0;
    const int w = int(m.value("width").toDouble() / scale), h = int(m.value("height").toDouble() / scale);
    return QRect(m.value("x").toInt() + left, m.value("y").toInt() + top, w - left - right, h - top - bottom);
}

PipController::PipController(AppState *state, Engine *engine, Settings *prefs, QSettings *store, QObject *parent)
    : QObject(parent), m_state(state), m_engine(engine), m_prefs(prefs), m_store(store) {
    for (const char *old : {"pipX", "pipY", "pipW", "pipH", "pipMonitor"}) m_store->remove(old);   // the first version saved its own default as if the person had chosen it
    m_placeTimer.setInterval(40);                         // fast: a window that has just opened is still TILED, shoving the others aside, until it is floated
    connect(&m_placeTimer, &QTimer::timeout, this, &PipController::tryPlace);
    connect(m_engine, &Engine::previewingChanged, this, [this] { rememberActive(); emit changed(); });
    connect(m_engine, &Engine::stateChanged, this, &PipController::onEngineState);
    m_holdTimer.setSingleShot(true);
    m_holdTimer.setInterval(qEnvironmentVariableIntValue("MOBS_HOLD_MS") ?: 180000);   // tests shorten it
    connect(&m_holdTimer, &QTimer::timeout, this, [this] {
        MobsLog::write("pip", "MOBS's window was kept up for a few minutes while going live: hiding it now");
        Notifier::send("MOBS: window hidden", "MOBS stepped out of the picture. SUPER + SHIFT + V stops the stream.");
        hideMainNow();
    });
    connect(m_state, &AppState::pickChanged, this, &PipController::changed);
    connect(m_state, &AppState::screenChanged, this, &PipController::changed);
    connect(m_state, &AppState::cameraChanged, this, &PipController::changed);
    connect(m_prefs, &Settings::changed, this, &PipController::changed);
}

PipController::~PipController() { if (m_mainHidden) restoreMain(); }

// Wanted whenever "screen + me" is showing its picture (ready with the preview on, armed, or a take in progress): Engine::previewing is exactly that.
bool PipController::wanted() const { return m_prefs->pip() && m_state->pick() == 2 && m_state->hasCamera() && m_engine->previewing(); }

// The chat card under the webcam: only where there is a chat feed to show (X's relay, or the demo) and the person has not turned it off.
bool PipController::chatWanted() const { return wanted() && m_prefs->chat() && m_chat && m_chat->configured(); }

void PipController::setChat(ChatModel *chat) {
    m_chat = chat;
    if (m_chat) {
        connect(m_chat, &ChatModel::statusChanged, this, &PipController::changed);
        connect(m_chat, &ChatModel::statusChanged, this, [this] { QTimer::singleShot(250, this, &PipController::resizeForChat); });   // after the interface has had its say
    }
}

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

void PipController::cameraProblem(const QString &what) { MobsLog::write("pip", "camera: " + what); }

void PipController::setCaptureSize(int width, int height) {
    m_captureSize = QSize(width, height);
    MobsLog::write("pip", QString("the picture is a %1x%2 screen").arg(width).arg(height));
}

// ---------------------------------------------------------------- Hyprland: float, pin, place; and getting MOBS's own window out of the picture

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
    if (m_reopenAskedAt && QDateTime::currentMSecsSinceEpoch() - m_reopenAskedAt < 5000) {   // this is the put-back after a close, not a first showing
        m_reopenAskedAt = 0;
        MobsLog::write("pip", "webcam window put back: it was closed during a take");
        Notifier::send("MOBS: your webcam window was closed", "You were off the stream for a moment. MOBS put it back.", true);
    }
    const QCameraFormat f = format();
    MobsLog::write("pip", testPattern() ? QString("webcam window shown (a stand-in picture, no camera)")
        : QString("webcam window shown: camera \"%1\", format %2x%3 at up to %4 fps").arg(device().description()).arg(f.resolution().width()).arg(f.resolution().height()).arg(int(f.maxFrameRate())));
    if (!onHyprland()) { MobsLog::write("pip", "(not on Hyprland: it is an ordinary window, move it yourself)"); return; }
    m_placeTimer.start();
}

void PipController::tryPlace() {
    if (!m_shown) { m_placeTimer.stop(); return; }
    const QJsonArray clients = hyprJson({"clients", "-j"}).array();
    const QString address = addressOf(clients, kTitle, QCoreApplication::applicationPid());
    if (address.isEmpty()) {
        if (++m_tries > 100) { m_placeTimer.stop(); MobsLog::write("pip", "gave up waiting for the compositor to list the webcam window"); }
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

    const bool chatOn = chatWanted();   // the dock has a chat card under the video: its size and its remembered place differ from the video alone
    m_placeKey = chatOn ? QStringLiteral("pipPlaceChat") : QStringLiteral("pipPlace");
    // Where: where the person last left it (if that monitor is still there), else the top-right of MOBS's own monitor, sized for the broadcast.
    QRect saved; QString savedMonitor;
    QJsonObject monitor;
    QRect rect;
    if (decodePlace(m_store->value(m_placeKey).toString(), &saved, &savedMonitor)) {
        monitor = monitorByName(monitors, savedMonitor);
        if (!monitor.isEmpty()) {
            if (chatOn && saved.height() < saved.width() * 9 / 16 + 160) saved.setHeight(saved.width() * 9 / 16 + chatHeightFor(saved.width()));   // too short to hold a chat card: give it one
            rect = fitInside(saved, monitor);
        }
    }
    if (rect.isNull()) {
        monitor = {};
        for (const QJsonValue &v : clients) {            // MOBS's own main window decides the monitor
            const QJsonObject c = v.toObject();
            if (c.value("pid").toInt() == QCoreApplication::applicationPid() && c.value("title").toString() != kTitle) { monitor = monitorById(monitors, c.value("monitor").toInt()); break; }
        }
        if (monitor.isEmpty()) for (const QJsonValue &v : monitors) if (v.toObject().value("focused").toBool()) monitor = v.toObject();
        if (monitor.isEmpty() && !monitors.isEmpty()) monitor = monitors.first().toObject();
        if (!monitor.isEmpty()) rect = defaultRect(monitor, defaultSize(monitor, chatOn));
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
    m_placed = rect;
    m_chatPlaced = chatOn;
    MobsLog::write("pip", QString("webcam window %1 placed at %2,%3 %4x%5 on %6 (floating, pinned to every workspace); SUPER + drag moves it, SUPER + right-drag resizes it")
        .arg(address).arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()).arg(monitor.value("name").toString()));
}

// The livestream is found once the person has pressed Go Live, so the chat card usually turns on AFTER the dock window is already on screen. The size of a
// floating window belongs to the compositor: Qt asking for a taller window is not honoured on Hyprland (the chat card then had no room and the dock showed no
// chat at all). So the window is resized here, keeping its corner and its width; the person's own size is kept when it already has room for a chat card.
QRect PipController::rectForChat(const QRect &now, bool chatOn, const QJsonObject &monitor) {
    const int videoH = now.width() * 9 / 16;
    if (chatOn && now.height() >= videoH + 160) return now;   // already tall enough to hold a chat card
    QRect want = now;
    want.setHeight(chatOn ? videoH + chatHeightFor(now.width()) : videoH);
    const QRect fitted = fitInside(want, monitor);
    return fitted.isNull() ? want : fitted;
}

void PipController::resizeForChat() {
    if (!m_shown || !onHyprland() || m_placed.isNull()) return;   // not placed yet: place() reads the chat state itself
    const bool chatOn = chatWanted();
    if (chatOn == m_chatPlaced) return;                            // the size already suits this state
    const QJsonArray clients = hyprJson({"clients", "-j"}).array();
    const QString address = addressOf(clients, kTitle, QCoreApplication::applicationPid());
    const QJsonObject c = clientAt(clients, address);
    if (address.isEmpty() || c.isEmpty()) return;
    const QJsonArray at = c.value("at").toArray(), size = c.value("size").toArray();
    const QJsonObject monitor = monitorById(hyprJson({"monitors", "-j"}).array(), c.value("monitor").toInt());
    if (at.size() < 2 || size.size() < 2 || monitor.isEmpty()) return;
    const QRect now(at[0].toInt(), at[1].toInt(), size[0].toInt(), size[1].toInt());
    const QRect want = rectForChat(now, chatOn, monitor);
    m_chatPlaced = chatOn;
    m_placeKey = chatOn ? QStringLiteral("pipPlaceChat") : QStringLiteral("pipPlace");
    if (want == now) { m_placed = now; return; }
    const QString win = QString("window=\"address:%1\"").arg(address);
    hyprEval(QString("hl.dispatch(hl.dsp.window.resize({x=%1,y=%2,%3}))").arg(want.width()).arg(want.height()).arg(win));
    hyprEval(QString("hl.dispatch(hl.dsp.window.move({x=%1,y=%2,%3}))").arg(want.x()).arg(want.y()).arg(win));
    m_placed = want;
    MobsLog::write("pip", QString("the chat card turned %1: webcam window resized to %2x%3 at %4,%5").arg(chatOn ? "on" : "off").arg(want.width()).arg(want.height()).arg(want.x()).arg(want.y()));
}

void PipController::windowHidden() {
    m_shown = false; m_placeTimer.stop();
    rememberWhereItWasLeft();
    reopenIfClosedDuringATake();
}

bool PipController::reopenAllowed(const QString &engineState, int reopensInTheLastMinute) {
    const bool taking = engineState == "starting" || engineState == "recording" || engineState == "paused";
    return taking && reopensInTheLastMinute < 3;
}

// The window went while a take is on and the picture still wants it: it was CLOSED (a stray SUPER + Q), not taken away by the take ending or the setting going off.
// Put it back, where the person last left it, and say so once it is really back (windowShown). Before a take, and after one, a closed window stays closed as always.
void PipController::reopenIfClosedDuringATake() {
    if (!wanted() || QCoreApplication::closingDown()) return;
    const QString st = m_engine->state();
    if (st != "starting" && st != "recording" && st != "paused") return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    while (!m_reopens.isEmpty() && now - m_reopens.first() > 60000) m_reopens.removeFirst();
    if (!reopenAllowed(st, int(m_reopens.size()))) {
        if (m_gaveUp) return;
        m_gaveUp = true;
        MobsLog::write("pip", "webcam window closed again and again during a take: MOBS leaves it closed");
        Notifier::send("MOBS: your webcam is off the stream", "You closed it again, so MOBS stopped putting it back. Stop and start the take, or turn the webcam window off in settings.", true);
        return;
    }
    m_reopens.append(now);
    m_reopenAskedAt = now;
    MobsLog::write("pip", "webcam window closed during a take: putting it back");
    QTimer::singleShot(300, this, [this] { if (wanted()) emit reopenRequested(); });   // a moment, so the compositor has finished with the old window
}

void PipController::rememberWhereItWasLeft() {
    if (!onHyprland()) return;
    const QJsonArray clients = hyprJson({"clients", "-j"}).array();
    const QJsonObject c = clientAt(clients, addressOf(clients, kTitle, QCoreApplication::applicationPid()));
    if (c.isEmpty()) return;
    const QJsonArray at = c.value("at").toArray(), size = c.value("size").toArray();
    const QJsonObject monitor = monitorById(hyprJson({"monitors", "-j"}).array(), c.value("monitor").toInt());
    if (at.size() < 2 || size.size() < 2 || monitor.isEmpty()) return;
    const QRect now(at[0].toInt(), at[1].toInt(), size[0].toInt(), size[1].toInt());
    // Only a place the person CHOSE is remembered (a move or a resize); where MOBS put it by itself is recomputed next time, for whatever screen is there then.
    if (m_placed.isValid() && (std::abs(now.x() - m_placed.x()) > 4 || std::abs(now.y() - m_placed.y()) > 4 || std::abs(now.width() - m_placed.width()) > 4 || std::abs(now.height() - m_placed.height()) > 4)) {
        m_store->setValue(m_placeKey, encodePlace(now, monitor.value("name").toString())); m_store->sync();
        MobsLog::write("pip", QString("webcam window left at %1,%2 %3x%4 on %5: remembered, you moved it").arg(now.x()).arg(now.y()).arg(now.width()).arg(now.height()).arg(monitor.value("name").toString()));
    } else MobsLog::write("pip", "webcam window closed where MOBS put it");
    m_prevActive.clear();
}

// A broadcast keeps MOBS's window up until the person says they pressed Go live in X: the steps for it are ON that window. A plain recording
// has nothing to finish, so it hides at once as before.
void PipController::holdMain() {
    m_held = true;
    emit heldChanged();
    m_holdTimer.start();
    MobsLog::write("pip", "going live: MOBS's window stays up until you press \"I'm live\" (or a few minutes pass)");
    Notifier::send("MOBS: now finish in X", "Open Live Studio, press New Livestream, pick your source, choose Public and press Go Live. Then press I'M LIVE in MOBS.");
}

void PipController::releaseHold() {
    m_holdTimer.stop();
    if (!m_held) return;
    m_held = false;
    emit heldChanged();
}

void PipController::hideMainNow() {
    const bool wasHeld = m_held;
    releaseHold();
    if (wasHeld && m_engine->state() == "recording" && !m_mainHidden) hideMain();
}

void PipController::onEngineState() {
    const QString st = m_engine->state();
    if (st == "idle") { m_gaveUp = false; m_reopens.clear(); }   // the next take starts with a clean slate
    if (st == "recording" && !m_mainHidden && !m_held) { if (m_engine->streamRequested() && m_prefs->stayOut()) holdMain(); else hideMain(); }
    else if (st == "idle") { releaseHold(); if (m_mainHidden) restoreMain(); }
    else if (st == "stopping") releaseHold();
}

// A screen capture shows everything on that screen, MOBS's own window included: a stream of MOBS showing the stream. So while a take records, MOBS's window
// steps aside to a hidden workspace (the hotkey SUPER + SHIFT + V still stops it) and comes back when the take ends.
void PipController::hideMain() {
    if (!m_prefs->stayOut() || !onHyprland()) return;
    if (m_state->pick() != 1 && m_state->pick() != 2) return;     // just-the-camera does not capture a screen with MOBS on it
    const QJsonArray clients = hyprJson({"clients", "-j"}).array();
    QJsonObject mine;
    for (const QJsonValue &v : clients) {
        const QJsonObject c = v.toObject();
        if (c.value("pid").toInt() == QCoreApplication::applicationPid() && c.value("title").toString() != kTitle) { mine = c; break; }
    }
    if (mine.isEmpty()) return;
    const QJsonArray monitors = hyprJson({"monitors", "-j"}).array();
    const QJsonObject monitor = monitorById(monitors, mine.value("monitor").toInt());
    const QString workspace = mine.value("workspace").toObject().value("name").toString();
    if (workspace.isEmpty() || workspace.startsWith("special")) return;
    if (!shouldHide(monitor, m_captureSize, monitors.size())) { MobsLog::write("pip", "MOBS's window is not on the monitor in the picture: left where it is"); return; }
    m_hiddenAddress = mine.value("address").toString(); m_homeWorkspace = workspace; m_mainHidden = true;
    hyprEval(QString("hl.dispatch(hl.dsp.window.move({workspace=\"special:mobs\",follow=false,window=\"address:%1\"}))").arg(m_hiddenAddress));
    MobsLog::write("pip", QString("MOBS's window hidden from the picture (on special:mobs); it returns to workspace %1 when the take ends. SUPER + SHIFT + V stops the take.").arg(workspace));
}

void PipController::restoreMain() {
    const QString address = m_hiddenAddress, workspace = m_homeWorkspace;
    m_mainHidden = false; m_hiddenAddress.clear(); m_homeWorkspace.clear();
    if (address.isEmpty() || !onHyprland()) return;
    if (clientAt(hyprJson({"clients", "-j"}).array(), address).isEmpty()) return;   // closed meanwhile
    hyprEval(QString("hl.dispatch(hl.dsp.window.move({workspace=\"%1\",follow=false,window=\"address:%2\"}))").arg(workspace, address));
    MobsLog::write("pip", QString("MOBS's window is back on workspace %1").arg(workspace));
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

int PipController::chatHeightFor(int width) { return qBound(220, int(width * 9 / 16 * 0.65), 480); }

QSize PipController::defaultSize(const QJsonObject &monitor, bool withChat) {
    const int w = qBound(480, int(usableArea(monitor).width() * 0.20), 1024);
    return QSize(w, w * 9 / 16 + (withChat ? chatHeightFor(w) : 0));   // the video keeps its 16:9; the chat card is added underneath
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

QString PipController::encodePlace(const QRect &rect, const QString &monitor) {
    return QString("%1,%2,%3,%4,%5").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()).arg(monitor);
}

bool PipController::decodePlace(const QString &text, QRect *rect, QString *monitor) {
    const QStringList p = text.split(',');
    if (p.size() != 5) return false;
    bool ok[4] = {false, false, false, false}; int v[4];
    for (int i = 0; i < 4; ++i) v[i] = p[i].toInt(&ok[i]);
    if (!ok[0] || !ok[1] || !ok[2] || !ok[3] || v[2] < 160 || v[3] < 90 || v[2] > 8000 || v[3] > 8000 || p[4].isEmpty()) return false;
    *rect = QRect(v[0], v[1], v[2], v[3]); *monitor = p[4];
    return true;
}

bool PipController::shouldHide(const QJsonObject &monitor, const QSize &captured, int monitorCount) {
    if (monitor.isEmpty()) return false;
    if (captured.isValid() && !captured.isEmpty()) return QSize(monitor.value("width").toInt(), monitor.value("height").toInt()) == captured;   // the monitor whose size is in the picture
    return monitorCount == 1;                                       // size not known yet: with one monitor there is no doubt
}
