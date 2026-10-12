#include "appstate.h"
#include "mobslog.h"
#include <QDesktopServices>
#include <QUrl>

// The guided setup's part of AppState: whether it is showing, the picture it asks for, the camera the person locked in, and opening X's pages.

void AppState::openSetup() {
    if (m_setupOpen) return;
    m_setupOpen = true;
    MobsLog::write("setup", "guided setup opened");
    emit setupChanged();
}

void AppState::closeSetup() {
    if (!m_setupOpen && !m_setupPreview) return;
    m_setupOpen = false; m_setupPreview = false;
    emit setupChanged();
}

void AppState::setSetupPreview(bool on) {
    if (on == m_setupPreview) return;
    m_setupPreview = on;
    emit setupChanged();
}

// "This is my camera". It is chosen first (only a camera MOBS can see can be chosen), then kept: ensureCamera() never swaps a locked camera
// for another one, so an unplugged BRIO is reported as unplugged instead of quietly becoming the laptop's webcam.
void AppState::lockCamera(const QString &path) {
    setCamera(path);
    if (path != m_camera) return;
    m_cameraLocked = true;
    m_lockedName = cameraName();
    m_settings->setValue("cameraLocked", true); m_settings->setValue("cameraLockedName", m_lockedName);
    m_settings->sync();
    MobsLog::write("setup", "camera locked in: " + m_lockedName);
    emit cameraChanged();
}

void AppState::unlockCamera() {
    if (!m_cameraLocked) return;
    m_cameraLocked = false;
    m_settings->setValue("cameraLocked", false); m_settings->sync();
    emit cameraChanged();
}

// Only X's own pages are ever opened from here: the guides send the person to Live Studio, and nowhere else.
bool AppState::openUrl(const QString &url) {
    const QUrl u(url);
    const bool ours = u.scheme() == "https" && (u.host() == "x.com" || u.host().endsWith(".x.com"));
    if (!ours) { MobsLog::write("setup", "refused to open a page that is not X's: " + u.host()); return false; }
    MobsLog::write("setup", "opening " + u.toString(QUrl::RemoveQuery));
    if (qEnvironmentVariable("MOBS_NO_OPEN") == "1") return true;   // tests: never open a browser on the desktop they run on
    return QDesktopServices::openUrl(u);
}
