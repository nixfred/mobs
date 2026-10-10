#include "appstate.h"
#include "control.h"
#include "engine.h"
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class FakeBackend : public RecorderBackend {
public:
    void begin(int pick, const QString &) override { began++; lastPick = pick; }
    void pause(bool p) override { pauseCalls++; lastPause = p; }
    void end() override { ended++; }
    int began = 0, ended = 0, pauseCalls = 0, lastPick = 0;
    bool lastPause = false;
};

// The slots are plain methods: D-Bus only exports them. Their logic is tested here, the bus itself in the VM.
class ControlTests : public QObject {
    Q_OBJECT
    QTemporaryDir dir;
private slots:
    void toggleStartsWhatWasPicked() {
        QSettings s(dir.path() + "/a.ini", QSettings::IniFormat); AppState st(&s); st.choose(1);
        FakeBackend b; Engine e(&b, dir.path()); Control c(&e, &st);
        c.Toggle();
        QCOMPARE(b.began, 1);
        QCOMPARE(e.state(), QString("starting"));
    }
    void toggleOnAFreshInstallRecordsTheScreen() {
        QSettings s(dir.path() + "/b.ini", QSettings::IniFormat); AppState st(&s);
        QCOMPARE(st.pick(), 0);
        FakeBackend b; Engine e(&b, dir.path()); Control c(&e, &st);
        c.Toggle();
        QCOMPARE(st.pick(), 1);          // the hotkey chose "my screen" instead of doing nothing
        QCOMPARE(b.lastPick, 1);
    }
    void secondToggleStops() {
        QSettings s(dir.path() + "/c.ini", QSettings::IniFormat); AppState st(&s); st.choose(1);
        FakeBackend b; Engine e(&b, dir.path()); Control c(&e, &st);
        c.Toggle(); emit b.started();
        c.Toggle();
        QCOMPARE(b.ended, 1);
        QCOMPARE(e.state(), QString("stopping"));
    }
    void pauseTogglesPauseAndResume() {
        QSettings s(dir.path() + "/d.ini", QSettings::IniFormat); AppState st(&s); st.choose(1);
        FakeBackend b; Engine e(&b, dir.path()); Control c(&e, &st);
        c.Pause(); QCOMPARE(b.pauseCalls, 0);            // nothing recording: ignored
        c.Toggle(); emit b.started();
        c.Pause(); QCOMPARE(e.state(), QString("paused"));
        c.Pause(); QCOMPARE(e.state(), QString("recording"));
    }
    void toggleWithACameraPickDoesNotRecord() {
        QSettings s(dir.path() + "/e.ini", QSettings::IniFormat); AppState st(&s); st.choose(2);
        FakeBackend b; Engine e(&b, dir.path()); Control c(&e, &st);
        c.Toggle();
        QCOMPARE(b.began, 0);            // camera sources are not wired: the engine says so rather than recording nothing
        QVERIFY(e.message().contains("not wired"));
    }
    void statusReportsStateAndTime() {
        QSettings s(dir.path() + "/f.ini", QSettings::IniFormat); AppState st(&s); st.choose(1);
        FakeBackend b; Engine e(&b, dir.path()); Control c(&e, &st);
        QCOMPARE(c.Status(), QString("idle"));
        c.Toggle(); emit b.started();
        QVERIFY2(c.Status().startsWith("recording 00:00:0"), qPrintable(c.Status()));
    }
};
QTEST_MAIN(ControlTests)
#include "tst_control.moc"
