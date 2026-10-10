#pragma once
#include <QObject>
#include <QSettings>

// Which screen MOBS shows and what the user picked. Remembered between launches.
// Screens so far: 1 = choose, 2 = ready, 3 = recording. (Review and the Pro drawer come later.)
class AppState : public QObject {
    Q_OBJECT
    Q_PROPERTY(int screen READ screen NOTIFY screenChanged)
    Q_PROPERTY(int pick READ pick NOTIFY pickChanged)
    Q_PROPERTY(QString pickName READ pickName NOTIFY pickChanged)
    Q_PROPERTY(QString displayText READ displayText CONSTANT)
public:
    explicit AppState(QSettings *settings, QObject *parent = nullptr);
    int screen() const { return m_recording ? 3 : m_screen; }
    int pick() const { return m_pick; }
    QString pickName() const;
    QString displayText() const;
    Q_INVOKABLE void choose(int pick);   // 1 my screen, 2 screen + me, 3 just my camera
    Q_INVOKABLE void back();             // ready -> choose
    void setRecording(bool recording);   // the engine is busy: show the recording screen
signals:
    void screenChanged();
    void pickChanged();
private:
    QSettings *m_settings;
    int m_screen = 1, m_pick = 0;
    bool m_recording = false;
};
