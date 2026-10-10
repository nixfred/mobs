#include "engine.h"
#include "review.h"
#include <QTemporaryDir>
#include <QFileInfo>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>
#include <limits>

// A backend that records nothing: the test plays the part of libobs by emitting its signals.
class FakeBackend : public RecorderBackend {
public:
    void begin(const RecordSpec &spec) override { began++; lastPick = spec.pick; lastPath = spec.path; lastSpec = spec; }
    void pause(bool p) override { pauseCalls++; lastPause = p; }
    void end() override { ended++; }
    void setPreview(bool on, const RecordSpec &spec) override { previewCalls++; previewOn = on; previewSpec = spec; }
    void setMicCheck(bool on, const AudioSpec &spec) override { checkCalls++; checking = on; checkedAudio = spec; }
    int checkCalls = 0; bool checking = false; AudioSpec checkedAudio;
    int previewCalls = 0; bool previewOn = false; RecordSpec previewSpec;
    int began = 0, ended = 0, pauseCalls = 0, lastPick = 0;
    bool lastPause = false;
    QString lastPath;
    RecordSpec lastSpec;
};

class EngineTests : public QObject {
    Q_OBJECT
    QTemporaryDir dir;
private slots:
    void immediateQuitIsOnlyAvailableWhileStopping() {
        FakeBackend b; Engine e(&b, dir.path()); QSignalSpy quit(&e, &Engine::forceQuitRequested);
        e.requestForceQuit(); QCOMPARE(quit.count(), 0);
        e.start(1); e.requestForceQuit(); QCOMPARE(quit.count(), 0);
        emit b.started(); e.requestForceQuit(); QCOMPARE(quit.count(), 0);
        e.stop(); QCOMPARE(e.state(), QString("stopping"));
        e.requestForceQuit(); QCOMPARE(quit.count(), 1);
        emit b.finished({}); e.requestForceQuit(); QCOMPARE(quit.count(), 1);
    }
    void failedPictureCheckerDoesNotLeaveAProcessBehind() {
        QTemporaryDir tools;
        QFile stub(tools.path() + "/ffmpeg"); QVERIFY(stub.open(QIODevice::WriteOnly));
        stub.write("#!/missing/mobs-test-interpreter\n"); stub.close();
        QVERIFY(stub.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const auto previous = qgetenv("PATH");
        const auto restore = qScopeGuard([&] { qputenv("PATH", previous); });
        qputenv("PATH", tools.path().toUtf8());
        QFile take(tools.path() + "/take.mp4"); QVERIFY(take.open(QIODevice::WriteOnly)); take.close();
        FakeBackend b; Engine e(&b, dir.path());
        emit b.finished(take.fileName());
        QTRY_VERIFY_WITH_TIMEOUT(e.findChildren<QProcess *>().isEmpty(), 1000);
    }
    void micCheckStopsBeforeRecordingAndOptionsAreSnapshotted() {
        FakeBackend b; Engine e(&b, dir.path()); AudioSpec a;
        a.micDevice = "usb"; a.micGainDb = -9; a.desktop = true;
        e.setAudioProvider([&] { return a; }); e.setHardwareProvider([] { return true; });
        e.checkMic(true); QVERIFY(e.micChecking()); QVERIFY(b.checking);
        QCOMPARE(b.checkedAudio.micDevice, QString("usb"));
        e.start(1); QVERIFY(!e.micChecking()); QVERIFY(!b.checking);
        QVERIFY(b.lastSpec.hardware); QVERIFY(e.recordingDesktop());
        QCOMPARE(b.lastSpec.audio.micGainDb, -9);
        a.micDevice = "changed"; a.desktop = false;
        QCOMPARE(e.recordingMicDevice(), QString("usb")); QVERIFY(e.recordingDesktop());
        const int calls = b.checkCalls; e.checkMic(true); QCOMPARE(b.checkCalls, calls);
    }
    void editorActionsClearAnIdlePromptButKeepLiveRecordingMessages() {
        FakeBackend b; Engine e(&b, dir.path());
        emit b.waitingForUser("save your edit"); e.clearIdleMessage(); QVERIFY(e.message().isEmpty());
        e.start(1); emit b.waitingForUser("choose a screen"); e.clearIdleMessage();
        QCOMPARE(e.message(), QString("choose a screen"));
    }
    void failedGuardStopsAnExistingMicCheck() {
        FakeBackend b; Engine e(&b, dir.path()); QString problem;
        e.setMicGuard([&] { return problem; }); e.checkMic(true);
        problem = "unplugged"; e.checkMic(true);
        QVERIFY(!e.micChecking()); QVERIFY(!b.checking); QCOMPARE(e.audioProblem(), problem);
        e.checkMic(false); QVERIFY(e.audioProblem().isEmpty());
        problem.clear(); e.checkMic(true); emit b.micCheckFailed("capture failed");
        QVERIFY(!e.micChecking()); QCOMPARE(e.audioProblem(), QString("capture failed"));
    }
    void audioMetersSanitizeBadValuesAndHardwareResultsAreVisible() {
        FakeBackend b; Engine e(&b, dir.path());
        emit b.audioLevels(std::numeric_limits<double>::quiet_NaN(), 40, false, true, "desktop only");
        QCOMPARE(e.micDb(), -60); QCOMPARE(e.desktopDb(), 12);
        QVERIFY(!e.micFrames()); QVERIFY(e.desktopFrames());
        QCOMPARE(e.audioProblem(), QString("desktop only"));
        emit b.hardwareChecked(true, "available");
        QVERIFY(e.hardwareKnown()); QVERIFY(e.nvencAvailable());
        QCOMPARE(e.hardwareMessage(), QString("available"));
    }
    void recordingNamesDoNotOverwriteTakesOrSidecars() {
        const QString stamp = "2026-10-10 00-00-00-123";
        const QString first = Engine::recordingPathFor(dir.path(), stamp);
        QFile f(first); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("original"); f.close();
        const QString second = Engine::recordingPathFor(dir.path(), stamp);
        QVERIFY(second != first);
        QVERIFY(Review::writePauses(second, {1.0}));
        const QString third = Engine::recordingPathFor(dir.path(), stamp);
        QVERIFY(third != second && third != first);
        QVERIFY(QFile::link(dir.path() + "/missing-target", third));
        QVERIFY(Engine::recordingPathFor(dir.path(), stamp) != third);
        QVERIFY(f.open(QIODevice::ReadOnly)); QCOMPARE(f.readAll(), QByteArray("original"));
    }
    void unsavedOrExportingReviewBlocksAllStarts() {
        FakeBackend b; Engine e(&b, dir.path());
        bool blocked = true;
        e.setStartGuard([&] { return blocked ? QString("save your edit before recording") : QString(); });
        e.start(1); e.toggle(1);
        QCOMPARE(b.began, 0); QCOMPARE(e.state(), QString("idle"));
        QVERIFY(e.message().contains("save your edit"));
        blocked = false; e.toggle(1); QCOMPARE(b.began, 1);
    }
    void expertOptionsAndRecordingSummaryAreSnapshotted() {
        FakeBackend b; Engine e(&b, dir.path());
        bool mic = false;
        e.setOptionsProvider([&] { return std::make_pair(17, mic); });
        e.setPresetProvider([] { return QString("fast"); });
        e.start(1);
        QCOMPARE(b.lastSpec.crf, 17); QCOMPARE(b.lastSpec.encoderPreset, QString("fast"));
        QVERIFY(!b.lastSpec.mic); QVERIFY(!e.recordingMic());
        QCOMPARE(e.recordingFolder(), dir.path());
        mic = true; QVERIFY(!e.recordingMic());
    }
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
        QVERIFY(t >= 0.2);
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
    // ---- preview ----------------------------------------------------------------------------------------------------
    void previewTurnsOnForTheScreenPickWithoutACamera() {
        FakeBackend b; Engine e(&b, dir.path());
        QSignalSpy sp(&e, &Engine::previewingChanged);
        e.setPreview(true, 1);
        QVERIFY(e.previewing());
        QVERIFY(b.previewOn);
        QCOMPARE(b.previewSpec.pick, 1);
        QCOMPARE(sp.count(), 1);
        e.setPreview(false, 1);
        QVERIFY(!e.previewing());
        QVERIFY(!b.previewOn);
    }
    void previewForACameraPickNeedsAUsableCamera() {
        FakeBackend b; Engine e(&b, dir.path());
        e.setPreview(true, 2);                          // no camera chosen: nothing to show, nothing started
        QVERIFY(!e.previewing());
        QCOMPARE(b.previewCalls, 0);
        CameraSpec cam; cam.path = "/dev/v4l/by-id/x-video-index0"; cam.corner = 0; cam.size = 2;
        e.setCameraProvider([cam] { return cam; });
        e.setPreview(true, 2);
        QVERIFY(e.previewing());
        QCOMPARE(b.previewSpec.camera.corner, 0);       // the placement goes through, for the backend to apply
        QCOMPARE(b.previewSpec.camera.size, 2);
    }
    void previewIsRepeatedSoTheBackendCanFollowChanges() {
        FakeBackend b; Engine e(&b, dir.path());
        CameraSpec cam; cam.path = "x";
        e.setCameraProvider([&cam] { return cam; });
        e.setPreview(true, 2);
        cam.corner = 1;
        e.setPreview(true, 2);                          // the user tapped another corner
        QCOMPARE(b.previewCalls, 2);
        QCOMPARE(b.previewSpec.camera.corner, 1);
        QVERIFY(e.previewing());
    }
    void switchingToAPickWithoutACameraTurnsThePreviewOffInTheBackend() {
        FakeBackend b; Engine e(&b, dir.path());
        CameraSpec cam; cam.path = "x";
        bool have = true;
        e.setCameraProvider([&] { return have ? cam : CameraSpec{}; });
        e.setPreview(true, 3);
        QVERIFY(b.previewOn);
        have = false;                                   // the camera was unplugged
        e.setPreview(true, 3);
        QVERIFY(!e.previewing());
        QVERIFY(!b.previewOn);                          // and the backend was told, not left showing a dead camera
    }
    void turningOffAnAlreadyOffPreviewDoesNotBotherTheBackend() {
        FakeBackend b; Engine e(&b, dir.path());
        e.setPreview(false, 1); e.setPreview(false, 1);
        QCOMPARE(b.previewCalls, 0);
    }
    void aPreviewFailureDoesNotEndARecording() {
        FakeBackend b; Engine e(&b, dir.path());
        e.start(1); emit b.started(); e.setPreview(true, 1);
        emit b.previewFailed("preview unavailable");
        QCOMPARE(e.state(), QString("recording")); QVERIFY(!e.previewing());
        QCOMPARE(e.message(), QString("preview unavailable"));
    }
    void previewNeverChangesTheRecordingState() {
        FakeBackend b; Engine e(&b, dir.path());
        e.setPreview(true, 1);
        QCOMPARE(e.state(), QString("idle"));
        e.start(1); emit b.started();
        e.setPreview(true, 1);
        QCOMPARE(e.state(), QString("recording"));
        e.setPreview(false, 1);
        QCOMPARE(e.state(), QString("recording"));
    }
    // ---- pause marks for the editor ----------------------------------------------------------------------------------
    void pausePointsAreWrittenBesideTheTakeForTheEditor() {
        QTemporaryDir d;
        const QString file = d.path() + "/take.mp4";
        QFile f(file); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("x"); f.close();
        FakeBackend b; Engine e(&b, d.path());
        e.start(1); emit b.started();
        QTest::qWait(300);  e.togglePause();                              // paused at about 0.3 s of recorded time
        QTest::qWait(200);  e.togglePause();                              // (the 0.2 s while paused is NOT counted)
        QTest::qWait(300);  e.togglePause();                              // paused again at about 0.6 s
        e.togglePause();
        e.stop(); emit b.finished(file);
        const QList<double> marks = Review::readPauses(file);
        QCOMPARE(marks.size(), 2);
        QVERIFY2(marks[0] > 0.2 && marks[0] < 0.5, qPrintable(QString::number(marks[0])));
        QVERIFY2(marks[1] > marks[0] + 0.2 && marks[1] < 1.0, qPrintable(QString::number(marks[1])));   // positions in the FINAL file
    }
    void noPausesMeansNoSidecarAndAMissingFileIsNotFatal() {
        QTemporaryDir d;
        const QString file = d.path() + "/clean.mp4";
        QFile f(file); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("x"); f.close();
        FakeBackend b; Engine e(&b, d.path());
        e.start(1); emit b.started(); e.stop(); emit b.finished(file);
        QVERIFY(!QFileInfo::exists(Review::sidecarPath(file)));
        e.start(1); emit b.started(); QTest::qWait(100); e.togglePause(); e.togglePause(); e.stop();
        emit b.finished(d.path() + "/never-existed.mp4");                 // the file is gone: no crash, no stray sidecar
        QVERIFY(!QFileInfo::exists(Review::sidecarPath(d.path() + "/never-existed.mp4")));
    }
    // ---- recording options from the Pro drawer ------------------------------------------------------------------------
    void theQualityAndMicSettingsReachTheBackendAtEveryStart() {
        FakeBackend b; Engine e(&b, dir.path());
        std::pair<int, bool> opts{28, false};
        e.setOptionsProvider([&] { return opts; });
        e.start(1);
        QCOMPARE(b.lastSpec.crf, 28);
        QVERIFY(!b.lastSpec.mic);
        emit b.started(); e.stop(); emit b.finished(QString());
        opts = {18, true};                                                // changed in the drawer between recordings
        e.start(1);
        QCOMPARE(b.lastSpec.crf, 18);
        QVERIFY(b.lastSpec.mic);
    }
    void theFolderSettingDecidesWhereTheFileGoesAtEveryStart() {
        FakeBackend b; Engine e(&b, dir.path());
        QTemporaryDir other;
        QString chosen = other.path() + "/one";
        e.setFolderProvider([&] { return chosen; });
        e.start(1);
        QVERIFY2(b.lastPath.startsWith(chosen + "/mobs "), qPrintable(b.lastPath));
        QVERIFY(QFileInfo(chosen).isDir());                               // made on demand
        emit b.started(); e.stop(); emit b.finished(QString());
        chosen = other.path() + "/two";
        e.start(1);
        QVERIFY(b.lastPath.startsWith(other.path() + "/two/mobs "));
    }
    void anUnusableFolderExplainsItselfAndRecordsNothing() {
        FakeBackend b; Engine e(&b, dir.path());
        e.setFolderProvider([] { return QString("/proc/not/a/place"); });
        e.start(1);
        QCOMPARE(b.began, 0);
        QCOMPARE(e.state(), QString("idle"));
        QVERIFY(e.message().contains("cannot save to"));
        QVERIFY(e.message().contains("EXPERT"));                             // and says where to fix it
    }
    void withoutProvidersTheDefaultsApply() {
        FakeBackend b; Engine e(&b, dir.path());
        e.start(1);
        QCOMPARE(b.lastSpec.crf, 23);
        QVERIFY(b.lastSpec.mic);
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
