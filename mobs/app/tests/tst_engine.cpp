#include "engine.h"
#include <QFileInfo>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

// A backend that records nothing: the test plays the part of libobs by emitting its signals.
class FakeBackend : public RecorderBackend {
public:
    void begin(const RecordSpec &spec) override { began++; lastPick = spec.pick; lastPath = spec.path; lastSpec = spec; }
    void pause(bool p) override { pauseCalls++; lastPause = p; }
    void end() override { ended++; }
    int began = 0, ended = 0, pauseCalls = 0, lastPick = 0;
    bool lastPause = false;
    QString lastPath;
    RecordSpec lastSpec;
};

class EngineTests : public QObject {
    Q_OBJECT
    QTemporaryDir dir;
private slots:
    void startGoesStartingThenRecording() {
        FakeBackend b; Engine e(&b, dir.path());
        QCOMPARE(e.state(), QString("idle"));
        e.start(1);
        QCOMPARE(e.state(), QString("starting"));
        QCOMPARE(b.began, 1);
        QVERIFY(b.lastPath.startsWith(dir.path() + "/mobs "));
        QVERIFY(b.lastPath.endsWith(".mp4"));
        QVERIFY(e.active());
        emit b.started();
        QCOMPARE(e.state(), QString("recording"));
    }
    void elapsedFreezesWhilePaused() {
        FakeBackend b; Engine e(&b, dir.path());
        e.start(1); emit b.started();
        QTest::qWait(400);
        QVERIFY2(e.elapsedSeconds() >= 0.3, qPrintable(QString::number(e.elapsedSeconds())));
        const double before = e.elapsedSeconds();
        e.togglePause();
        QCOMPARE(e.state(), QString("paused"));
        QVERIFY(b.lastPause);
        const double frozen = e.elapsedSeconds();
        QVERIFY2(frozen >= before - 0.02, "pausing must KEEP the time already recorded");
        QTest::qWait(400);
        QVERIFY(qAbs(e.elapsedSeconds() - frozen) < 0.02);       // the clock must not run while paused
        e.togglePause();
        QCOMPARE(e.state(), QString("recording"));
        QVERIFY(!b.lastPause);
        QTest::qWait(300);
        QVERIFY(e.elapsedSeconds() > frozen + 0.2);               // and it must carry on from where it stopped
    }
    void stopSavesTheFile() {
        FakeBackend b; Engine e(&b, dir.path());
        QSignalSpy saved(&e, &Engine::saved);
        e.start(1); emit b.started();
        e.stop();
        QCOMPARE(e.state(), QString("stopping"));
        QCOMPARE(b.ended, 1);
        emit b.finished("/tmp/x.mp4");
        QCOMPARE(e.state(), QString("idle"));
        QCOMPARE(e.lastFile(), QString("/tmp/x.mp4"));
        QCOMPARE(saved.count(), 1);
        QVERIFY(e.message().contains("saved"));
    }
    void cancellingBeforeTheFirstFrameSavesNothing() {
        FakeBackend b; Engine e(&b, dir.path());
        QSignalSpy saved(&e, &Engine::saved);
        e.start(1);
        e.stop();                       // still "starting": the picker is open
        QCOMPARE(b.ended, 1);
        emit b.finished(QString());
        QCOMPARE(e.state(), QString("idle"));
        QCOMPARE(saved.count(), 0);
        QVERIFY(e.message().contains("cancelled"));
    }
    void failureReturnsToIdleWithTheReason() {
        FakeBackend b; Engine e(&b, dir.path());
        e.start(1);
        emit b.failed("no encoder");
        QCOMPARE(e.state(), QString("idle"));
        QCOMPARE(e.message(), QString("no encoder"));
        e.start(1);                     // and the user can simply try again
        QCOMPARE(b.began, 2);
    }
    void failureWhileRecordingStopsTheClock() {
        FakeBackend b; Engine e(&b, dir.path());
        e.start(1); emit b.started(); QTest::qWait(250);
        emit b.failed("disk full");
        QCOMPARE(e.state(), QString("idle"));
        const double t = e.elapsedSeconds();
        QTest::qWait(300);
        QCOMPARE(e.elapsedSeconds(), t);
    }
    void cameraPicksWithoutACameraAskForOne() {
        FakeBackend b; Engine e(&b, dir.path());
        QSignalSpy needed(&e, &Engine::cameraNeeded);
        e.start(2); QCOMPARE(b.began, 0); QVERIFY(e.message().contains("choose a camera")); QCOMPARE(e.state(), QString("idle"));
        e.start(3); QCOMPARE(b.began, 0);
        QCOMPARE(needed.count(), 2);                    // so the UI can open the camera panel
    }
    void cameraPicksWithACameraRecordWithItsSettings() {
        FakeBackend b; Engine e(&b, dir.path());
        CameraSpec cam; cam.path = "/dev/v4l/by-id/usb-x-video-index0"; cam.mjpg720 = true; cam.corner = 1; cam.size = 2;
        e.setCameraProvider([cam] { return cam; });
        e.start(2);
        QCOMPARE(b.began, 1);
        QCOMPARE(b.lastSpec.pick, 2);
        QCOMPARE(b.lastSpec.camera.path, cam.path);
        QCOMPARE(b.lastSpec.camera.corner, 1);
        QCOMPARE(b.lastSpec.camera.size, 2);
        QVERIFY(b.lastSpec.camera.mjpg720);
    }
    void thePlainScreenPickNeverNeedsACamera() {
        FakeBackend b; Engine e(&b, dir.path());
        e.start(1);
        QCOMPARE(b.began, 1);
        QVERIFY(!b.lastSpec.camera.valid());
    }
    void unknownPicksAreIgnored() {
        FakeBackend b; Engine e(&b, dir.path());
        e.start(0); e.start(4);
        QCOMPARE(b.began, 0);
        QVERIFY(e.message().isEmpty());
    }
    void startIsIgnoredWhileBusy() {
        FakeBackend b; Engine e(&b, dir.path());
        e.start(1); e.start(1);
        QCOMPARE(b.began, 1);
        emit b.started(); e.start(1);
        QCOMPARE(b.began, 1);
    }
    void pauseOnlyWhenRecording() {
        FakeBackend b; Engine e(&b, dir.path());
        e.togglePause(); QCOMPARE(b.pauseCalls, 0);       // idle
        e.start(1); e.togglePause(); QCOMPARE(b.pauseCalls, 0);   // starting
    }
    void toggleStartsThenStops() {
        FakeBackend b; Engine e(&b, dir.path());
        e.toggle(1); QCOMPARE(b.began, 1);
        emit b.started();
        e.toggle(1); QCOMPARE(b.ended, 1);                // the Alt+Shift+V behaviour
    }
    void stopWhileIdleOrStoppingDoesNothing() {
        FakeBackend b; Engine e(&b, dir.path());
        e.stop(); QCOMPARE(b.ended, 0);
        e.start(1); emit b.started(); e.stop(); e.stop();
        QCOMPARE(b.ended, 1);
    }
    void elapsedIsFormattedAsClockTime() {
        QCOMPARE(Engine::formatElapsed(0), QString("00:00:00"));
        QCOMPARE(Engine::formatElapsed(59.9), QString("00:00:59"));
        QCOMPARE(Engine::formatElapsed(3725), QString("01:02:05"));
    }
    // ---- the black-recording check (needs ffmpeg; skipped without it) -----------------------------------------------
    static bool makeVideo(const QString &file, const QString &source) {
        QProcess p;
        p.start("ffmpeg", {"-nostdin", "-v", "error", "-y", "-f", "lavfi", "-i", source, "-t", "2", "-pix_fmt", "yuv420p", file});
        return p.waitForFinished(20000) && p.exitCode() == 0;
    }
    void looksBlackJudgesTheStill() {
        QVERIFY(Engine::looksBlack(QByteArray(576, '\0')));
        QVERIFY(Engine::looksBlack(QByteArray(576, '\2')));
        QVERIFY(!Engine::looksBlack(QByteArray(576, '\x30')));
        QByteArray one(576, '\0'); one[100] = '\xff';
        QVERIFY(!Engine::looksBlack(one));              // a single bright pixel is content, not black
        QByteArray mixed(576, '\0'); for (int i = 0; i < 576; i += 2) mixed[i] = char(0xE0);
        QVERIFY(!Engine::looksBlack(mixed));            // bright and black pixels together: content
        QVERIFY(!Engine::looksBlack(QByteArray()));     // no data: never warn
    }
    void aBlackRecordingGetsAWarning() {
        if (QStandardPaths::findExecutable("ffmpeg").isEmpty()) QSKIP("ffmpeg not installed");
        const QString f = dir.path() + "/black.mp4";
        QVERIFY(makeVideo(f, "color=c=black:s=320x180:r=10"));
        FakeBackend b; Engine e(&b, dir.path());
        QSignalSpy checked(&e, &Engine::checkedFile);
        e.start(1); emit b.started(); e.stop(); emit b.finished(f);
        QTRY_COMPARE_WITH_TIMEOUT(checked.count(), 1, 15000);
        QVERIFY(checked.first().at(1).toBool());
        QVERIFY(e.warning().contains("black"));
        QVERIFY(e.warning().contains("region"));
    }
    void aNormalRecordingGetsNoWarning() {
        if (QStandardPaths::findExecutable("ffmpeg").isEmpty()) QSKIP("ffmpeg not installed");
        const QString f = dir.path() + "/real.mp4";
        QVERIFY(makeVideo(f, "testsrc2=s=320x180:r=10"));
        FakeBackend b; Engine e(&b, dir.path());
        QSignalSpy checked(&e, &Engine::checkedFile);
        e.start(1); emit b.started(); e.stop(); emit b.finished(f);
        QTRY_COMPARE_WITH_TIMEOUT(checked.count(), 1, 15000);
        QVERIFY(!checked.first().at(1).toBool());
        QVERIFY(e.warning().isEmpty());
    }
    void startingAgainClearsTheOldWarning() {
        if (QStandardPaths::findExecutable("ffmpeg").isEmpty()) QSKIP("ffmpeg not installed");
        const QString f = dir.path() + "/black2.mp4";
        QVERIFY(makeVideo(f, "color=c=black:s=320x180:r=10"));
        FakeBackend b; Engine e(&b, dir.path());
        QSignalSpy checked(&e, &Engine::checkedFile);
        e.start(1); emit b.started(); e.stop(); emit b.finished(f);
        QTRY_COMPARE_WITH_TIMEOUT(checked.count(), 1, 15000);
        QVERIFY(!e.warning().isEmpty());
        const QString g = dir.path() + "/real2.mp4";
        QVERIFY(makeVideo(g, "testsrc2=s=320x180:r=10"));
        e.start(1); emit b.started(); e.stop(); emit b.finished(g);
        QTRY_VERIFY_WITH_TIMEOUT(e.warning().isEmpty(), 5000);   // a good recording must not inherit the last warning
    }
    void aLateStartedSignalAfterStopIsIgnored() {
        FakeBackend b; Engine e(&b, dir.path());
        e.start(1); e.stop();
        emit b.started();               // backend raced: must not resurrect the recording
        QCOMPARE(e.state(), QString("stopping"));
    }
};
QTEST_MAIN(EngineTests)
#include "tst_engine.moc"
