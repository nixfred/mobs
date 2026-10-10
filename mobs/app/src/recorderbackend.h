#pragma once
#include <QImage>
#include <QObject>
#include <QString>

struct CameraSpec {
    QString path;            // stable /dev/v4l/by-id path, or any non-empty string when a test source stands in
    bool mjpg720 = false;    // ask the camera for 1280x720 MJPEG
    int corner = 3;          // 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right (used by "screen + me")
    int size = 1;            // 0 small, 1 medium, 2 large
    bool pip = false;        // "screen + me" with the webcam as a WINDOW on the screen: libobs captures only the screen, which then contains the webcam
    int x = -1, y = -1;      // the webcam dragged to a free place: its top-left on the 1920x1080 canvas. -1 = use the corner above
    int guestCrop[4] = {0, 0, 0, 0};   // "the show" only: percent trimmed off the guest's window: left, top, right, bottom
    bool valid() const { return !path.isEmpty(); }
};
struct AudioSpec {
    QString micDevice = "default";
    int micGainDb = 0;
    bool desktop = false;
    QString desktopDevice = "default";
    int desktopGainDb = -6;
};
// Where a live take is also sent. The key is a password: it lives only in this struct, in memory, for the length of one take.
struct StreamSpec {
    bool on = false;
    QString url;             // rtmp:// or rtmps:// server address
    QString key;             // the stream key; never logged, never shown
    int kbps = 6000;         // constant video bitrate (the server wants a fixed rate, unlike the file's quality-based one)
    int height = 1080;       // 720 or 1080 (16:9); the file is always 1080
};
struct RecordSpec {
    int pick = 1;            // 1 my screen, 2 screen + me, 3 just my camera, 4 the show (screen + guest's window + me)
    QString path;            // the file to write
    CameraSpec camera;
    int crf = 23;            // x264 CRF / NVIDIA CQP (lower = bigger, better); from the quality setting
    bool mic = true;         // record the selected microphone
    QString encoderPreset = "veryfast"; // x264 CPU/compression tradeoff
    AudioSpec audio;
    bool hardware = false;
    StreamSpec stream;       // when on, the take is also broadcast; the file is still saved as the backup
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
    // Forget which screen (and guest window) was chosen, so the next time MOBS asks again. Never touches a take in progress.
    virtual void forgetCapture() {}
    virtual void setMicCheck(bool on, const AudioSpec &audio) { Q_UNUSED(on); Q_UNUSED(audio); }
    virtual void checkHardware() { emit hardwareChecked(false, "hardware detection is not supported by this backend"); }
signals:
    void audioLevels(double micDb, double desktopDb, bool micFrames, bool desktopFrames, const QString &problem);
    void micCheckFailed(const QString &why);
    void hardwareChecked(bool nvenc, const QString &message);
    void previewFrame(const QImage &image);     // ~15 per second while previewing, 640x360
    void previewFailed(const QString &why);    // preview failure leaves the recording state alone
    // The broadcast, which can fail and come back without touching the file. state: "connecting", "live", "reconnecting", "failed", "ended".
    // detail is a plain sentence for "failed" and "reconnecting", empty otherwise.
    void streamState(const QString &state, const QString &detail);
    void streamStats(int kbps, int droppedFrames);   // about once a second while live
    void waitingForUser(const QString &what);   // e.g. the screen picker is open
    void started();                             // frames are really being recorded
    void failed(const QString &why);            // nothing is recording any more
    void finished(const QString &path);         // the file is complete; empty path = cancelled before it started
};
