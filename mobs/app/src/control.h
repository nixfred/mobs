#pragma once
#include <QObject>
#include <QString>

class Engine;
class AppState;

// The hotkey door: org.mobs.Control on the session bus. A Hyprland bind runs
//   busctl --user call org.mobs.Control /org/mobs/Control org.mobs.Control Toggle
// from any application. If MOBS is not running, D-Bus launches it (see dist/org.mobs.Control.service).
class Control : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mobs.Control")
public:
    Control(Engine *engine, AppState *state, QObject *parent = nullptr);
    // Takes the bus name and exports the object. False (and a warning) if another MOBS already owns the name.
    bool registerOnSessionBus();
public slots:
    void Toggle();          // idle: start recording what was picked (screen if nothing was); recording: stop and save
    void Pause();           // recording: pause; paused: resume
    QString Status();       // "idle", or "recording 00:00:12", "paused 00:00:12", ...
private:
    Engine *m_engine;
    AppState *m_state;
};
