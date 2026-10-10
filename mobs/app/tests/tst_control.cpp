#include "appstate.h"
#include "control.h"
#include "engine.h"
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class FakeBackend : public RecorderBackend {
public:
    void begin(const RecordSpec &spec) override { began++; lastPick = spec.pick; lastSpec = spec; }
    void pause(bool p) override { pauseCalls++; lastPause = p; }
    void end() override { ended++; }
    int began = 0, ended = 0, pauseCalls = 0, lastPick = 0;
    bool lastPause = false;
    RecordSpec lastSpec;
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
    void toggleOnAFreshInstallWithoutACameraRecordsTheScreen() {
        QSettings s(dir.path() + "/b.ini", QSettings::IniFormat); AppState st(&s, nullptr, [] { return QList<CameraInfo>(); });
        QCOMPARE(st.pick(), 1);          // a first run is never "nothing picked": the hotkey must always have something to do
        FakeBackend b; Engine e(&b, dir.path()); Control c(&e, &st);
        c.Toggle();
        QCOMPARE(b.began, 1); QCOMPARE(b.lastPick, 1);
    }
    void toggleOnAFreshInstallWithACameraRecordsScreenPlusMe() {
        QSettings s(dir.path() + "/b2.ini", QSettings::IniFormat);
        AppState st(&s, nullptr, [] { return QList<CameraInfo>{{"/dev/v4l/by-id/usb-brio-video-index0", "Logitech BRIO", true}}; });
        QCOMPARE(st.pick(), 2);          // "All I want is screen and me"
        FakeBackend b; Engine e(&b, dir.path()); e.setCameraProvider([&] { return st.cameraSpec(); }); Control c(&e, &st);
        c.Toggle();
        QCOMPARE(b.began, 1); QCOMPARE(b.lastPick, 2);
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
    void toggleWithACameraPickButNoCameraDoesNotRecord() {
        QSettings s(dir.path() + "/e.ini", QSettings::IniFormat); AppState st(&s); st.choose(2);
        FakeBackend b; Engine e(&b, dir.path()); Control c(&e, &st);
        c.Toggle();
        QCOMPARE(b.began, 0);            // no camera chosen: the engine asks for one rather than recording nothing
        QVERIFY(e.message().contains("choose a camera"));
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
