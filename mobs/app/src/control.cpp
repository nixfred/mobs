#include "control.h"
#include "appstate.h"
#include "engine.h"
#include "mobslog.h"
#include <QDBusConnection>
#include <QDBusError>
#include <QDebug>

Control::Control(Engine *engine, AppState *state, QObject *parent) : QObject(parent), m_engine(engine), m_state(state) {}

bool Control::registerOnSessionBus() {
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) { qWarning() << "control: no session bus, the hotkey will not work"; return false; }
    if (!bus.registerService("org.mobs.Control")) {
        qWarning() << "control: org.mobs.Control is already taken (another MOBS is running?):" << bus.lastError().message();
        return false;
    }
    return bus.registerObject("/org/mobs/Control", this, QDBusConnection::ExportAllSlots);
}

void Control::Toggle() {
    MobsLog::write("control", "hotkey: toggle");
    if (!m_engine->active() && m_state->pick() == 0) m_state->choose(1);   // hotkey on a fresh install: my screen
    m_engine->toggle(m_state->pick());
}

void Control::Pause() { MobsLog::write("control", "hotkey: pause"); m_engine->togglePause(); }

QString Control::Status() {
    const QString s = m_engine->state();
    return s == "idle" ? s : s + " " + m_engine->elapsedText();
}
