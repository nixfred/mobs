#include "appstate.h"
#include <QGuiApplication>
#include "placement.h"
#include <QScreen>
#include <QVariantMap>

AppState::AppState(QSettings *settings, QObject *parent, CameraScan scan) : QObject(parent), m_settings(settings), m_scan(std::move(scan)) {
    m_camera = m_settings->value("camera").toString();
    m_corner = qBound(0, m_settings->value("corner", 3).toInt(), 3);
    m_size = qBound(0, m_settings->value("size", 1).toInt(), 2);
    m_cams = m_scan();
    const int saved = m_settings->value("pick", 0).toInt();
    if (saved >= 1 && saved <= 3) { m_pick = saved; m_screen = 2; }   // returning user lands on "ready"
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
    if (pick != 1) ensureCamera();                    // a camera pick must not start with "no camera" if one exists
    if (pick != m_pick) { m_pick = pick; m_settings->setValue("pick", pick); m_settings->sync(); emit pickChanged(); }
    if (m_screen != 2) { m_screen = 2; emit screenChanged(); }
}

void AppState::back() {
    if (m_screen != 1) { m_screen = 1; emit screenChanged(); }
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
    return spec;
}

void AppState::ensureCamera() {
    if (currentCamera() || m_cams.isEmpty()) return;
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
    m_camera = path;
    m_settings->setValue("camera", path);
    m_settings->sync();
    emit cameraChanged();
}

void AppState::setCorner(int corner) {
    if (corner < 0 || corner > 3 || corner == m_corner) return;
    m_corner = corner;
    m_settings->setValue("corner", corner);
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

QRectF AppState::cameraRectNormalized() const {
    const QRect r = Placement::cameraRect(m_corner, m_size);
    return QRectF(double(r.x()) / Placement::kCanvasW, double(r.y()) / Placement::kCanvasH,
                  double(r.width()) / Placement::kCanvasW, double(r.height()) / Placement::kCanvasH);
}

void AppState::setCameraPanel(bool open) {
    if (open == m_panel) return;
    m_panel = open;
    emit cameraPanelChanged();
}
