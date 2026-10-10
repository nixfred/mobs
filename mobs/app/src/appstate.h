#pragma once
#include "camera.h"
#include "recorderbackend.h"
#include <QObject>
#include <QRectF>
#include <QSettings>
#include <QVariantList>
#include <functional>

// Which screen MOBS shows and what the user picked. Remembered between launches.
// Screens so far: 1 = choose, 2 = ready, 3 = recording. (Review and the Pro drawer come later.)
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
    Q_PROPERTY(bool cameraPanel READ cameraPanel NOTIFY cameraPanelChanged)
public:
    using CameraScan = std::function<QList<CameraInfo>()>;
    explicit AppState(QSettings *settings, QObject *parent = nullptr, CameraScan scan = [] { return CameraScanner::scan(); });
    int screen() const { return m_recording ? 3 : m_screen; }
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
    CameraSpec cameraSpec() const;       // what the recorder needs; invalid when no connected camera is chosen
    Q_INVOKABLE void choose(int pick);   // 1 my screen, 2 screen + me, 3 just my camera
    Q_INVOKABLE void back();             // ready -> choose
    void setRecording(bool recording);   // the engine is busy: show the recording screen
    Q_INVOKABLE void refreshCameras();   // look again (a webcam may have been plugged in)
    Q_INVOKABLE void setCamera(const QString &path);
    Q_INVOKABLE void setCorner(int corner);
    Q_INVOKABLE void setSize(int size);
    Q_INVOKABLE QRectF cameraRectNormalized() const;   // 0..1 of the canvas: the SAME rectangle the recording uses
    Q_INVOKABLE void setCameraPanel(bool open);
signals:
    void screenChanged();
    void camerasChanged();
    void cameraChanged();
    void cameraPanelChanged();
    void pickChanged();
private:
    const CameraInfo *currentCamera() const;
    void ensureCamera();
    QSettings *m_settings;
    CameraScan m_scan;
    QList<CameraInfo> m_cams;
    QString m_camera;
    int m_corner = 3, m_size = 1;
    bool m_panel = false;
    int m_screen = 1, m_pick = 0;
    bool m_recording = false;
};
