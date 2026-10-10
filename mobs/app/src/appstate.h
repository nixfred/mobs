#pragma once
#include "camera.h"
#include "recorderbackend.h"
#include <QObject>
#include <QRectF>
#include <QSettings>
#include <QVariantList>
#include <functional>

// Which screen MOBS shows and what the user picked. Remembered between launches.
// Screens: 1 = choose, 2 = ready, 3 = recording, 4 = review (the Pro drawer comes later). Recording wins over review.
class AppState : public QObject {
    Q_OBJECT
    Q_PROPERTY(int screen READ screen NOTIFY screenChanged)
    Q_PROPERTY(int pick READ pick NOTIFY pickChanged)
    Q_PROPERTY(QString pickName READ pickName NOTIFY pickChanged)
    Q_PROPERTY(QString displayText READ displayText CONSTANT)
    Q_PROPERTY(QVariantList cameras READ cameras NOTIFY camerasChanged)
    Q_PROPERTY(QString camera READ camera NOTIFY cameraChanged)          // stable device path, "" = none
    Q_PROPERTY(QString cameraName READ cameraName NOTIFY cameraChanged)
    Q_PROPERTY(bool hasCamera READ hasCamera NOTIFY cameraChanged)
    Q_PROPERTY(int corner READ corner NOTIFY cameraChanged)              // 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right
    Q_PROPERTY(int size READ size NOTIFY cameraChanged)                  // 0 small, 1 medium, 2 large
    Q_PROPERTY(QVariantList guestCrop READ guestCrop NOTIFY cameraChanged)    // "the show": percent trimmed off the guest's window (left, top, right, bottom)
    Q_PROPERTY(bool cameraPanel READ cameraPanel NOTIFY cameraPanelChanged)
    Q_PROPERTY(int expertTab READ expertTab NOTIFY proChanged)
    Q_PROPERTY(bool pro READ pro NOTIFY proChanged)                       // the Pro drawer is open
    Q_PROPERTY(bool preview READ preview NOTIFY previewChanged)          // show what will be recorded (off by default)
public:
    using CameraScan = std::function<QList<CameraInfo>()>;
    explicit AppState(QSettings *settings, QObject *parent = nullptr, CameraScan scan = [] { return CameraScanner::scan(); });
    int screen() const { return m_recording ? 3 : m_reviewing ? 4 : m_screen; }
    int pick() const { return m_pick; }
    QString pickName() const;
    QString displayText() const;
    QVariantList cameras() const;
    QString camera() const { return m_camera; }
    QString cameraName() const;
    bool hasCamera() const { return currentCamera() != nullptr; }
    int corner() const { return m_corner; }
    int size() const { return m_size; }
    bool cameraPanel() const { return m_panel; }
    bool preview() const { return m_preview; }
    int expertTab() const { return m_expertTab; }
    Q_INVOKABLE void openAudio();
    Q_INVOKABLE void openLive();         // EXPERT > live: the server address and stream key
    bool pro() const { return m_pro; }
    CameraSpec cameraSpec() const;       // what the recorder needs; invalid when no connected camera is chosen
    Q_INVOKABLE void choose(int pick);   // 1 my screen, 2 screen + me, 3 just my camera
    Q_INVOKABLE void back();             // ready -> choose
    void setRecording(bool recording);   // the engine is busy: show the recording screen
    void setReviewing(bool reviewing);   // a take is open in the editor: show the review screen
    Q_INVOKABLE void refreshCameras();   // look again (a webcam may have been plugged in)
    Q_INVOKABLE void setCamera(const QString &path);
    Q_INVOKABLE void setCorner(int corner);
    Q_INVOKABLE void setSize(int size);
    Q_INVOKABLE QRectF cameraRectNormalized() const;   // 0..1 of the canvas: the SAME rectangle the recording uses
    Q_INVOKABLE QVariantMap showRectsNormalized() const;   // {main, guest, me} for "the show", 0..1 of the canvas
    QVariantList guestCrop() const;
    Q_INVOKABLE void setGuestCrop(int edge, int percent);  // edge 0 left, 1 top, 2 right, 3 bottom; 0..60, and opposite edges never meet
    Q_INVOKABLE void setCameraPanel(bool open);
    Q_INVOKABLE void setPreview(bool on);
    Q_INVOKABLE void setPro(bool open);
signals:
    void screenChanged();
    void camerasChanged();
    void cameraChanged();
    void cameraPanelChanged();
    void previewChanged();
    void proChanged();
    void pickChanged();
private:
    const CameraInfo *currentCamera() const;
    void ensureCamera();
    QSettings *m_settings;
    CameraScan m_scan;
    QList<CameraInfo> m_cams;
    QString m_camera;
    int m_corner = 1, m_size = 1;
    int m_guestCrop[4] = {0, 0, 0, 0};
    bool m_panel = false, m_preview = true, m_pro = false;
    int m_expertTab = 0;
    int m_screen = 1, m_pick = 0;
    bool m_recording = false, m_reviewing = false;
};
