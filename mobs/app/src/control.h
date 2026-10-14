#pragma once
#include <QObject>
#include <QString>
#include <functional>

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

    // The name this copy answers to. MOBS_CONTROL_NAME replaces it, so a test never touches the real hotkey door.
    static QString serviceName();
    // One copy at a time: a second copy would open the webcam the first one holds, take and stream with the same key
    // (X drops one of the two) and the hotkey would reach only the first. "" when no copy is running; otherwise what the
    // running copy says it is doing ("idle", "recording 00:12:34"), or "not answering" when it owns the name but is hung.
    static QString runningCopy();
    // What the kill switch does (the webcam window's controller owns it: it ends the livestream on X through the chat view, then stops the video).
    void setKillHandler(std::function<void()> kill) { m_kill = std::move(kill); }
    // What to tell the person who launched a second copy, from what runningCopy() returned.
    static QString secondCopyNotice(const QString &status);
public slots:
    void Toggle();          // idle: start recording what was picked (screen if nothing was); recording: stop and save
    void Pause();           // recording: pause; paused: resume
    QString Status();       // "idle", or "recording 00:00:12", "paused 00:00:12", ...
    void Kill();            // the kill switch: the video stops now and the livestream on X is ended (a plain stop when nothing is on air)
private:
    Engine *m_engine;
    AppState *m_state;
    std::function<void()> m_kill;
};
