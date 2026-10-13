#include "control.h"
#include "appstate.h"
#include "engine.h"
#include "mobslog.h"
#include "notifier.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>

Control::Control(Engine *engine, AppState *state, QObject *parent) : QObject(parent), m_engine(engine), m_state(state) {}

QString Control::serviceName() { return qEnvironmentVariable("MOBS_CONTROL_NAME", "org.mobs.Control"); }

QString Control::runningCopy() {
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface() || !bus.interface()->isServiceRegistered(serviceName())) return QString();
    QDBusInterface other(serviceName(), "/org/mobs/Control", "org.mobs.Control", bus);
    other.setTimeout(2000);
    const QDBusReply<QString> status = other.call("Status");
    return status.isValid() && !status.value().isEmpty() ? status.value() : QStringLiteral("not answering");
}

QString Control::secondCopyNotice(const QString &status) {
    if (status == "idle")
        return "MOBS is already open and is not recording. Look for its window. This second copy was not started.";
    if (status == "not answering")
        return "MOBS is already running but is not answering. Close it from its own window before you start another. This second copy was not started.";
    return QString("MOBS is already running a take (%1). Its window is hidden so it stays out of the picture, and SUPER + SHIFT + V stops the take. "
                   "This second copy was not started, so the webcam stays with the first.").arg(status);
}

bool Control::registerOnSessionBus() {
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) { qWarning() << "control: no session bus, the hotkey will not work"; return false; }
    if (!bus.registerService(serviceName())) {
        qWarning() << "control: org.mobs.Control is already taken (another MOBS is running?):" << bus.lastError().message();
        return false;
    }
    return bus.registerObject("/org/mobs/Control", this, QDBusConnection::ExportAllSlots);
}

void Control::Toggle() {
    MobsLog::write("control", "hotkey: toggle");
    // The setup guide covers the whole window and takes every key for itself: a take started now would run with nothing to see and no key to stop it but this one.
    // (Stopping a take that is already running is always allowed.)
    if (!m_engine->active() && m_state->setupOpen()) {
        MobsLog::write("control", "hotkey: not starting a take while the setup guide is open");
        Notifier::send("MOBS: finish the setup guide first", "The guide is still open, so no recording was started. Finish it, or press \"skip the guide\", then press the keys again.", true);
        return;
    }
    if (!m_engine->active() && m_state->pick() == 0) m_state->choose(1);   // hotkey on a fresh install: my screen
    m_engine->toggle(m_state->pick());
}

void Control::Pause() { MobsLog::write("control", "hotkey: pause"); m_engine->togglePause(); }

QString Control::Status() {
    const QString s = m_engine->state();
    return s == "idle" ? s : s + " " + m_engine->elapsedText();
}
