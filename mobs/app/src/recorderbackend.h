#pragma once
#include <QImage>
#include <QObject>
#include <QString>

struct CameraSpec {
    QString path;            // stable /dev/v4l/by-id path, or any non-empty string when a test source stands in
    bool mjpg720 = false;    // ask the camera for 1280x720 MJPEG
    int corner = 3;          // 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right (used by "screen + me")
    int size = 1;            // 0 small, 1 medium, 2 large
    bool valid() const { return !path.isEmpty(); }
};
struct RecordSpec {
    int pick = 1;            // 1 my screen, 2 screen + me, 3 just my camera
    QString path;            // the file to write
    CameraSpec camera;
};

// The narrow door between MOBS and whatever actually records. Today the only real implementation is libobs
// (ObsBackend); keeping this interface small is what lets the engine be swapped (or faked in tests) later.
// All calls and signals happen on the Qt main thread. begin() and end() return at once and finish by signal.
class RecorderBackend : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void begin(const RecordSpec &spec) = 0;
    virtual void pause(bool paused) = 0;
    virtual void end() = 0;   // stop (or cancel, if recording has not really started yet)
    // Show what would be recorded, without recording. Call again with a new spec when the pick, camera or placement
    // changes. A recording that starts afterwards reuses the same scene (no second screen-share dialog).
    virtual void setPreview(bool on, const RecordSpec &spec) { Q_UNUSED(on); Q_UNUSED(spec); }
signals:
    void previewFrame(const QImage &image);     // ~15 per second while previewing, 640x360
    void waitingForUser(const QString &what);   // e.g. the screen picker is open
    void started();                             // frames are really being recorded
    void failed(const QString &why);            // nothing is recording any more
    void finished(const QString &path);         // the file is complete; empty path = cancelled before it started
};
