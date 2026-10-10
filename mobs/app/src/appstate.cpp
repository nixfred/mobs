#include "appstate.h"
#include <QGuiApplication>
#include <QScreen>

AppState::AppState(QSettings *settings, QObject *parent) : QObject(parent), m_settings(settings) {
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
