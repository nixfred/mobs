#pragma once
#include <QObject>
#include <QSettings>

// Which screen MOBS shows and what the user picked. Remembered between launches.
// Screens so far: 1 = choose, 2 = ready. (Recording, review and the Pro drawer come later.)
class AppState : public QObject {
    Q_OBJECT
    Q_PROPERTY(int screen READ screen NOTIFY screenChanged)
    Q_PROPERTY(int pick READ pick NOTIFY pickChanged)
    Q_PROPERTY(QString pickName READ pickName NOTIFY pickChanged)
    Q_PROPERTY(QString displayText READ displayText CONSTANT)
    Q_PROPERTY(QString note READ note NOTIFY noteChanged)
public:
    explicit AppState(QSettings *settings, QObject *parent = nullptr);
    int screen() const { return m_screen; }
    int pick() const { return m_pick; }
    QString pickName() const;
    QString displayText() const;
    QString note() const { return m_note; }
    Q_INVOKABLE void choose(int pick);   // 1 my screen, 2 screen + me, 3 just my camera
    Q_INVOKABLE void back();             // ready -> choose
    Q_INVOKABLE void record();           // not wired to OBS yet: says so instead of pretending
signals:
    void screenChanged();
    void pickChanged();
    void noteChanged();
private:
    QSettings *m_settings;
    int m_screen = 1, m_pick = 0;
    QString m_note;
};
