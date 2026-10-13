#include "appstate.h"
#include "mobslog.h"
#include <QGuiApplication>
#include "placement.h"
#include <QScreen>
#include <cmath>
#include <QVariantMap>

AppState::AppState(QSettings *settings, QObject *parent, CameraScan scan) : QObject(parent), m_settings(settings), m_scan(std::move(scan)) {
    m_camera = m_settings->value("camera").toString();
    m_corner = qBound(0, m_settings->value("corner", 1).toInt(), 3);   // top-right: where Fred wants his face on a stream
    m_size = qBound(0, m_settings->value("size", 1).toInt(), 2);
    bool xOk = false, yOk = false;
    const int cx = m_settings->value("camX", -1).toInt(&xOk), cy = m_settings->value("camY", -1).toInt(&yOk);
    if (xOk && yOk && cx >= 0 && cy >= 0 && cx < Placement::kCanvasW && cy < Placement::kCanvasH) { m_camX = cx; m_camY = cy; }
    m_preview = m_settings->value("preview", true).toBool();   // on: a recorder (above all a broadcaster) that cannot be seen is one you cannot trust
    m_cameraLocked = m_settings->value("cameraLocked", false).toBool();
    m_lockedName = m_settings->value("cameraLockedName").toString();
    m_cams = m_scan();
    int saved = m_settings->value("pick", 0).toInt();
    if (saved == 4) saved = 2;              // an older MOBS had a fourth pick, "the show", which is gone: screen + me is the closest
    if (saved >= 1 && saved <= 3) { m_pick = saved; m_screen = 2; }   // returning user lands on "ready"
    else {                                  // first run: no three cards to puzzle over. The whole screen and your camera in a corner is what most people want.
        m_pick = m_cams.isEmpty() ? 1 : 2; m_screen = 2;
        if (m_pick != 1) ensureCamera();
    }
}

QString AppState::pickName() const {
    switch (m_pick) {
    case 1: return "my screen";
    case 2: return "screen + me";
    case 3: return "just my camera";
    default: return {};
    }
}

QString AppState::displayText() const {
    const QScreen *s = QGuiApplication::primaryScreen();
    if (!s) return "no display";
    const QSize px = s->size() * s->devicePixelRatio();
    return QString("%1  %2x%3").arg(s->name()).arg(px.width()).arg(px.height());
}

void AppState::choose(int pick) {
    if (pick < 1 || pick > 3) return;
    // What to share is for the NEXT take: a running take keeps the scene it started with, while the webcam window, the header and the screen all follow the pick,
    // so a click on a "what to share" button in EXPERT during a take used to close the webcam window on the stream.
    if (m_recording) { MobsLog::write("app", QString("share choice %1 asked for during a take: it waits for the next one").arg(pick)); return; }
    if (pick != 1) ensureCamera();                    // a camera pick must not start with "no camera" if one exists
    if (pick != m_pick) MobsLog::write("app", QString("pick -> %1").arg(pick));
    if (pick != m_pick) { m_pick = pick; m_settings->setValue("pick", pick); m_settings->sync(); emit pickChanged(); }
    if (m_screen != 2) { m_screen = 2; emit screenChanged(); }
}

void AppState::back() {
    if (m_screen != 1) { m_screen = 1; emit screenChanged(); }
}

void AppState::setReviewing(bool reviewing) {
    if (reviewing == m_reviewing) return;
    m_reviewing = reviewing;
    emit screenChanged();
}

void AppState::setRecording(bool recording) {
    if (recording == m_recording) return;
    m_recording = recording;
    emit screenChanged();
}

const CameraInfo *AppState::currentCamera() const {
    for (const CameraInfo &c : m_cams) if (c.path == m_camera) return &c;
    return nullptr;
}

QString AppState::cameraName() const {
    const CameraInfo *c = currentCamera();
    return c ? c->name : QString();
}

QVariantList AppState::cameras() const {
    QVariantList out;
    for (const CameraInfo &c : m_cams) out.append(QVariantMap{{"path", c.path}, {"name", c.name}});
    return out;
}

CameraSpec AppState::cameraSpec() const {
    CameraSpec spec;
    if (const CameraInfo *c = currentCamera()) { spec.path = c->path; spec.mjpg720 = c->mjpg720; }
    spec.corner = m_corner;
    spec.size = m_size;
    spec.x = m_camX; spec.y = m_camY;
    return spec;
}

void AppState::ensureCamera() {
    if (m_cameraLocked || currentCamera() || m_cams.isEmpty()) return;   // a camera the person locked in is never replaced by another one on its own
    m_camera = m_cams.first().path;
    m_settings->setValue("camera", m_camera);
    m_settings->sync();
    emit cameraChanged();
}

void AppState::refreshCameras() {
    const auto next = m_scan();
    if (next == m_cams) return;
    const bool hadCurrent = currentCamera() != nullptr;
    m_cams = next;
    emit camerasChanged();
    if (hadCurrent != (currentCamera() != nullptr)) emit cameraChanged();   // the chosen camera was unplugged (or came back)
}

void AppState::setCamera(const QString &path) {
    bool known = false;
    for (const CameraInfo &c : m_cams) known = known || c.path == path;
    if (!known || path == m_camera) return;
    MobsLog::write("app", "camera -> " + path);
    m_camera = path;
    m_settings->setValue("camera", path);
    if (m_cameraLocked) { m_lockedName = cameraName(); m_settings->setValue("cameraLockedName", m_lockedName); }   // picking another one on purpose moves the lock with it
    m_settings->sync();
    emit cameraChanged();
}

void AppState::setCorner(int corner) {
    if (corner < 0 || corner > 3 || (corner == m_corner && m_camX < 0)) return;
    MobsLog::write("app", QString("webcam corner -> %1").arg(corner));
    m_corner = corner;
    m_camX = m_camY = -1;                            // a corner tap puts a dragged webcam back on that corner
    m_settings->setValue("corner", corner);
    m_settings->remove("camX"); m_settings->remove("camY");
    m_settings->sync();
    emit cameraChanged();
}

void AppState::setSize(int size) {
    if (size < 0 || size > 2 || size == m_size) return;
    m_size = size;
    m_settings->setValue("size", size);
    m_settings->sync();
    emit cameraChanged();
}

void AppState::moveCamera(double nx, double ny, bool done) {
    const QRect size = Placement::cameraRect(0, m_size);
    const int x = qBound(0, int(std::lround(nx * Placement::kCanvasW)), Placement::kCanvasW - size.width());
    const int y = qBound(0, int(std::lround(ny * Placement::kCanvasH)), Placement::kCanvasH - size.height());
    const bool moved = x != m_camX || y != m_camY;
    m_camX = x; m_camY = y;
    if (done) MobsLog::write("app", QString("webcam dragged to %1,%2").arg(x).arg(y));
    if (done) { m_settings->setValue("camX", x); m_settings->setValue("camY", y); m_settings->sync(); }
    if (moved) emit cameraChanged();
}

QRectF AppState::cameraRectNormalized() const {
    const QRect r = m_camX >= 0 ? Placement::cameraRectAt(m_camX, m_camY, m_size) : Placement::cameraRect(m_corner, m_size);
    return QRectF(double(r.x()) / Placement::kCanvasW, double(r.y()) / Placement::kCanvasH,
                  double(r.width()) / Placement::kCanvasW, double(r.height()) / Placement::kCanvasH);
}

void AppState::setCameraPanel(bool open) {
    if (open == m_panel) return;
    m_panel = open;
    emit cameraPanelChanged();
}

void AppState::setPreview(bool on) {
    if (on == m_preview) return;
    MobsLog::write("app", QString("preview chip -> %1").arg(on ? "on" : "off"));
    m_preview = on;
    m_settings->setValue("preview", on);
    m_settings->sync();
    emit previewChanged();
}

void AppState::setPro(bool open) {
    if (open == m_pro) return;
    if (open) setCameraPanel(false);
    else m_expertTab = 0;
    m_pro = open;
    emit proChanged();
}

void AppState::openAudio() {
    m_expertTab = 2;
    if (m_pro) emit proChanged(); else setPro(true);
}

void AppState::openLive() {
    m_expertTab = 6;
    if (m_pro) emit proChanged(); else setPro(true);
}
