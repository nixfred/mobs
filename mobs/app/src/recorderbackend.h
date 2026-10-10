#pragma once
#include <QObject>
#include <QString>

// The narrow door between MOBS and whatever actually records. Today the only real implementation is libobs
// (ObsBackend); keeping this interface small is what lets the engine be swapped (or faked in tests) later.
// All calls and signals happen on the Qt main thread. begin() and end() return at once and finish by signal.
class RecorderBackend : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    // pick: 1 my screen, 2 screen + me, 3 just my camera. path: the file to write.
    virtual void begin(int pick, const QString &path) = 0;
    virtual void pause(bool paused) = 0;
    virtual void end() = 0;   // stop (or cancel, if recording has not really started yet)
signals:
    void waitingForUser(const QString &what);   // e.g. the screen picker is open
    void started();                             // frames are really being recorded
    void failed(const QString &why);            // nothing is recording any more
    void finished(const QString &path);         // the file is complete; empty path = cancelled before it started
};
