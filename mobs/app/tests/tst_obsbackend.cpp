// Integration test with the REAL libobs. It needs a Wayland session, so it skips itself everywhere else:
//   QT_QPA_PLATFORM=wayland MOBS_LIBOBS_TEST=1 ./tst_obsbackend      (run in the Test Drive VM)
#include "obsbackend.h"
#include "placement.h"
#include <QFileInfo>
#include <QGuiApplication>
#include <QMetaMethod>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <obs.h>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <memory>

// Integration fixtures never capture the real microphone.
static RecordSpec silentSpec(int pick, const QString &path) {
    RecordSpec spec; spec.mic = false; spec.pick = pick; spec.path = path; spec.mic = false; return spec;
}

static double durationOf(const QString &file) {
    QProcess p;
    p.start("ffprobe", {"-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", file});
    p.waitForFinished(10000);
    return p.readAllStandardOutput().trimmed().toDouble();
}

// One decoded still from 1 s in, 192x108 RGB: each pixel stands for 10x10 canvas pixels.
static QByteArray stillRgb(const QString &file) {
    QProcess p;
    p.start("ffmpeg", {"-nostdin", "-v", "error", "-ss", "1", "-i", file, "-frames:v", "1", "-vf", "scale=192:108", "-f", "rawvideo", "-pix_fmt", "rgb24", "-"});
    p.waitForFinished(20000);
    return p.readAllStandardOutput();
}
static bool greenAt(const QByteArray &f, int cx, int cy) {   // canvas coordinates
    const int i = ((cy / 10) * 192 + (cx / 10)) * 3;
    if (i + 2 >= f.size()) return false;
    const int r = (unsigned char)f[i], g = (unsigned char)f[i + 1];
    return g > r + 50;      // the stand-in camera is green, the screen is red
}
static bool redAt(const QByteArray &f, int cx, int cy) {
    const int i = ((cy / 10) * 192 + (cx / 10)) * 3;
    if (i + 2 >= f.size()) return false;
    const int r = (unsigned char)f[i], g = (unsigned char)f[i + 1];
    return r > g + 50;
}
static bool blueAt(const QByteArray &f, int cx, int cy) {
    const int i = ((cy / 10) * 192 + (cx / 10)) * 3;
    if (i + 2 >= f.size()) return false;
    return (unsigned char)f[i + 2] > (unsigned char)f[i] + 100;
}
static bool darkAt(const QByteArray &f, int cx, int cy) {      // the show's backdrop
    const int i = ((cy / 10) * 192 + (cx / 10)) * 3;
    if (i + 2 >= f.size()) return false;
    return (unsigned char)f[i] < 70 && (unsigned char)f[i + 1] < 70 && (unsigned char)f[i + 2] < 70;
}
static int pixelsIn(const QByteArray &f, const QRect &canvasRect, bool (*test)(unsigned char, unsigned char, unsigned char)) {   // counts over the 192x108 still
    int n = 0;
    for (int y = canvasRect.top() / 10; y < canvasRect.bottom() / 10; ++y)
        for (int x = canvasRect.left() / 10; x < canvasRect.right() / 10; ++x) {
            const int i = (y * 192 + x) * 3;
            if (i + 2 < f.size() && test((unsigned char)f[i], (unsigned char)f[i + 1], (unsigned char)f[i + 2])) ++n;
        }
    return n;
}
static int greenPixels(const QByteArray &f) {
    int n = 0;
    for (int i = 0; i + 2 < f.size(); i += 3) if ((unsigned char)f[i + 1] > (unsigned char)f[i] + 50) ++n;
    return n;
}


// RAII restores test overrides even if a Qt assertion returns early.
struct AudioFixture {
    QTemporaryDir dir;
    AudioFixture() {
        QProcess g; g.start("ffmpeg", {"-nostdin", "-v", "error", "-f", "lavfi", "-i", "sine=frequency=440:duration=8", dir.path() + "/tone.wav"});
        valid = g.waitForFinished(15000) && g.exitCode() == 0;
        qputenv("MOBS_TEST_AUDIO", ("file:" + dir.path() + "/tone.wav").toUtf8());
        qputenv("MOBS_TEST_DESKTOP_AUDIO", ("file:" + dir.path() + "/tone.wav").toUtf8());
    }
    ~AudioFixture() { qunsetenv("MOBS_TEST_AUDIO"); qunsetenv("MOBS_TEST_DESKTOP_AUDIO"); }
    bool valid = false;
};
static bool received(const QSignalSpy &spy, int index) {
    for (const auto &args : spy) if (args[index].toBool()) return true;
    return false;
}
static double audioRms(const QString &file) {
    QProcess p; p.start("ffmpeg", {"-nostdin", "-v", "error", "-ss", "0.5", "-i", file, "-t", "0.8", "-vn", "-ac", "1", "-f", "f32le", "-"});
    if (!p.waitForFinished(15000) || p.exitCode()) return -1;
    const auto bytes = p.readAllStandardOutput(); double sum = 0;
    for (int i = 0; i + 4 <= bytes.size(); i += 4) { float sample; std::memcpy(&sample, bytes.constData() + i, 4); sum += sample * sample; }
    return bytes.size() >= 4 ? std::sqrt(sum / (bytes.size() / 4)) : -1;
}

// A pretend X: ffmpeg waiting on this machine for one RTMP publisher, so a test broadcast never leaves the computer.
struct RtmpReceiver {
    QProcess p;
    RtmpReceiver(const QString &file, int port, const QString &key) {
        p.start("ffmpeg", {"-nostdin", "-v", "warning", "-listen", "1", "-i", QString("rtmp://127.0.0.1:%1/live/%2").arg(port).arg(key), "-c", "copy", "-f", "flv", file});
        p.waitForStarted(5000); QTest::qWait(800);
    }
    ~RtmpReceiver() { if (p.state() != QProcess::NotRunning) { p.kill(); p.waitForFinished(2000); } }
};
static QString probe(const QString &file, const QStringList &args) {
    QProcess p; p.start("ffprobe", QStringList{"-v", "error"} + args + QStringList{file});
    p.waitForFinished(15000); return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}
static QStringList statesOf(const QSignalSpy &spy) { QStringList out; for (const auto &a : spy) out << a[0].toString(); return out; }

class ObsBackendTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        if (qEnvironmentVariable("MOBS_LIBOBS_TEST") != "1") QSKIP("set MOBS_LIBOBS_TEST=1 on a Wayland session to run");
        if (!QGuiApplication::platformName().contains("wayland")) QSKIP("needs QT_QPA_PLATFORM=wayland");
        qputenv("MOBS_TEST_SOURCE", "colour");   // no screen picker in a test
    }
    void micCheckDeliversLevelsWithoutSavingOrOpeningVideoSources() {
        AudioFixture fixture; QVERIFY(fixture.valid); ObsBackend b(fixture.dir.path() + "/token");
        QSignalSpy levels(&b, &RecorderBackend::audioLevels), failed(&b, &RecorderBackend::micCheckFailed);
        b.setMicCheck(true, AudioSpec{});
        QTRY_VERIFY_WITH_TIMEOUT(received(levels, 2), 6000); QVERIFY(failed.isEmpty());
        auto *mic = obs_get_output_source(1); QVERIFY(mic); obs_source_release(mic);
        auto *video = obs_get_output_source(0); QVERIFY(!video);
        QVERIFY(!obs_get_output_by_name("mobs recording"));
        b.setMicCheck(false, AudioSpec{});
        QVERIFY(!obs_get_output_source(1));
        levels.clear(); QTest::qWait(300); QVERIFY(levels.isEmpty());
    }
    void missingAudioFramesAreExplained() {
        AudioFixture fixture; QVERIFY(fixture.valid);
        qputenv("MOBS_TEST_AUDIO", "file:/does/not/exist.wav");
        ObsBackend b(fixture.dir.path() + "/token"); QSignalSpy levels(&b, &RecorderBackend::audioLevels);
        b.setMicCheck(true, AudioSpec{});
        QTRY_VERIFY_WITH_TIMEOUT(!levels.isEmpty() && levels.last()[4].toString().contains("not sending"), 6500);
        b.setMicCheck(false, AudioSpec{});
    }
    void microphoneGainChangesTheSavedSound() {
        AudioFixture fixture; QVERIFY(fixture.valid); double rms[2]{};
        ObsBackend b(fixture.dir.path() + "/token");
        for (int i = 0; i < 2; ++i) {
            QSignalSpy levels(&b, &RecorderBackend::audioLevels), started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), failed(&b, &RecorderBackend::failed);
            auto spec = silentSpec(1, fixture.dir.path() + QString("/gain%1.mp4").arg(i));
            spec.mic = true; spec.audio.micGainDb = i ? -12 : 0;
            b.begin(spec); QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 15000);
            QTRY_VERIFY_WITH_TIMEOUT(received(levels, 2), 6000);
            QTest::qWait(1500);
            double peak = -60; for (const auto &args : levels) if (args[2].toBool()) peak = std::max(peak, args[0].toDouble());
            QVERIFY2(std::abs(peak - (i ? -30 : -18)) < 2, qPrintable(QString("meter peak %1").arg(peak)));
            b.end(); QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 15000);
            QVERIFY(failed.isEmpty()); rms[i] = audioRms(spec.path); QVERIFY(rms[i] > 0.001);
        }
        const double ratio = rms[1] / rms[0];
        QVERIFY2(ratio > 0.20 && ratio < 0.32, qPrintable(QString("-12 dB RMS ratio %1").arg(ratio)));
    }
    void desktopAudioWorksWithTheMicrophoneOff() {
        AudioFixture fixture; QVERIFY(fixture.valid); ObsBackend b(fixture.dir.path() + "/token");
        QSignalSpy levels(&b, &RecorderBackend::audioLevels), started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), failed(&b, &RecorderBackend::failed);
        auto spec = silentSpec(1, fixture.dir.path() + "/desktop.mp4"); spec.audio.desktop = true;
        b.begin(spec); QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(received(levels, 3), 6000);
        QVERIFY(!received(levels, 2)); QVERIFY(!obs_get_output_source(1));
        QTest::qWait(1500); b.end(); QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 15000);
        QVERIFY(failed.isEmpty()); QVERIFY(audioRms(spec.path) > 0.02);
        QVERIFY(!obs_get_output_source(2));
    }
    void bothAudioSourcesMixIntoTheSavedTrack() {
        AudioFixture fixture; QVERIFY(fixture.valid); ObsBackend b(fixture.dir.path() + "/token");
        QSignalSpy levels(&b, &RecorderBackend::audioLevels), started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), failed(&b, &RecorderBackend::failed);
        auto spec = silentSpec(1, fixture.dir.path() + "/mixed.mp4"); spec.mic = true; spec.audio.desktop = true;
        b.begin(spec); QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(received(levels, 2) && received(levels, 3), 6000);
        QTest::qWait(1500); b.end(); QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 15000);
        QVERIFY(failed.isEmpty()); QVERIFY(audioRms(spec.path) > 0.02);
    }
    void hardwareChoiceEitherRecordsOrExplainsItsUnavailability() {
        QTemporaryDir d; ObsBackend b(d.path() + "/token");
        QSignalSpy checked(&b, &RecorderBackend::hardwareChecked); b.checkHardware(); QCOMPARE(checked.count(), 1);
        qInfo() << "NVIDIA H.264 available:" << checked.first()[0].toBool();
        QSignalSpy started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), failed(&b, &RecorderBackend::failed);
        auto spec = silentSpec(1, d.path() + "/gpu.mp4"); spec.hardware = true;
        b.begin(spec);
        if (!checked.first()[0].toBool()) {
            QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 15000);
            QVERIFY2(failed.first()[0].toString().contains("NVIDIA"), qPrintable(failed.first()[0].toString())); QVERIFY(started.isEmpty());
            return;
        }
        for (int take = 0; take < 3; ++take) {
            if (take) { started.clear(); finished.clear(); spec.hardware = take != 1;
                spec.path = d.path() + QString("/switch%1.mp4").arg(take); b.begin(spec); }
            QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 15000);
            QTest::qWait(1500); b.end(); QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 15000);
            QVERIFY2(failed.isEmpty(), qPrintable(failed.isEmpty() ? QString() : failed.first()[0].toString()));
            QVERIFY(durationOf(spec.path) > 1); QVERIFY(stillRgb(spec.path).size() == 192 * 108 * 3);
        }
    }
    void aFileCreatedWhileCaptureStartsIsNeverOverwritten() {
        QTemporaryDir d; ObsBackend b(d.path() + "/token");
        QSignalSpy started(&b, &RecorderBackend::started), failed(&b, &RecorderBackend::failed);
        const QString path = d.path() + "/conflict.mp4";
        b.begin(silentSpec(1, path));
        QFile existing(path); QVERIFY(existing.open(QIODevice::WriteOnly)); existing.write("keep my recording"); existing.close();
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 5000);
        QVERIFY(failed.first()[0].toString().contains("already exists")); QVERIFY(started.isEmpty());
        QVERIFY(existing.open(QIODevice::ReadOnly)); QCOMPARE(existing.readAll(), QByteArray("keep my recording"));
    }
    void expertEncodingProducesAPlayableFile() {
        QTemporaryDir d; ObsBackend b(d.path() + "/token");
        QSignalSpy started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), failed(&b, &RecorderBackend::failed);
        auto spec = silentSpec(1, d.path() + "/expert.mp4"); spec.crf = 17; spec.encoderPreset = "fast";
        b.begin(spec); QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 15000);
        QTest::qWait(2500); b.end(); QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 15000);   // a wide margin: a busy machine must not turn 1.2 s of recording into a failure
        QVERIFY(failed.isEmpty()); QVERIFY(durationOf(spec.path) > 1.0);
    }
    void unexpectedOutputFailureIsReported() {
        QTemporaryDir d;
        ObsBackend b(d.path() + "/token");
        QSignalSpy started(&b, &RecorderBackend::started), failed(&b, &RecorderBackend::failed), finished(&b, &RecorderBackend::finished);
        b.begin(silentSpec(1, d.path() + "/error.mp4"));
        QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 15000);
        auto *output = obs_get_output_by_name("mobs recording");
        QVERIFY(output);
        obs_output_set_last_error(output, "test disk full");
        obs_output_signal_stop(output, OBS_OUTPUT_NO_SPACE);
        obs_output_release(output);
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 3000);
        QVERIFY(failed.first().first().toString().contains("test disk full"));
        QVERIFY(finished.isEmpty());
    }
    void recordsPausesAndSavesAValidFile() {
        QTemporaryDir d;
        ObsBackend b(d.path() + "/token");
        QSignalSpy started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), failed(&b, &RecorderBackend::failed);
        const QString path = d.path() + "/t.mp4";
        b.begin(silentSpec(1, path));
        QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 15000);
        QTest::qWait(1500);
        b.pause(true);  QTest::qWait(1500);
        b.pause(false); QTest::qWait(1500);
        b.end();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 20000);
        QVERIFY2(failed.isEmpty(), "backend reported a failure");
        QCOMPARE(finished.first().first().toString(), path);
        QVERIFY(QFileInfo(path).size() > 1000);
        const double dur = durationOf(path);
        // 1.5 s + 1.5 s recorded, 1.5 s paused: about 3 s, clearly less than the 4.5 s wall time.
        QVERIFY2(dur > 2.2 && dur < 3.9, qPrintable(QString("duration %1").arg(dur)));
    }
    void recordsAgainInTheSameProcess() {        // teardown must leave libobs reusable
        QTemporaryDir d;
        ObsBackend b(d.path() + "/token");
        for (int i = 0; i < 2; ++i) {
            QSignalSpy started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished);
            const QString path = d.path() + QString("/r%1.mp4").arg(i);
            b.begin(silentSpec(1, path));
            QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 15000);
            QTest::qWait(1200);
            b.end();
            QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 20000);
            QVERIFY(QFileInfo(path).size() > 1000);
        }
    }
    // ---- webcam placement, checked on the real pixels of the saved file -------------------------------------------------
    QByteArray recordWithCamera(ObsBackend &b, const QString &dir, int pick, int corner, int size) {
        qputenv("MOBS_TEST_COLOUR", "FF0000FF");           // red "screen"
        qputenv("MOBS_TEST_CAMERA", "colour");              // green "camera"
        QSignalSpy started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), failed(&b, &RecorderBackend::failed);
        RecordSpec spec; spec.mic = false; spec.pick = pick; spec.path = dir + QString("/cam-%1-%2-%3.mp4").arg(pick).arg(corner).arg(size);
        spec.camera.path = "test"; spec.camera.corner = corner; spec.camera.size = size;
        b.begin(spec);
        started.wait(15000);
        if (started.isEmpty()) return {};
        QTest::qWait(1800);
        b.end();
        finished.wait(20000);
        if (finished.isEmpty() || !failed.isEmpty()) return {};
        return stillRgb(spec.path);
    }
    void theCameraLandsInTheChosenCorner() {
        QTemporaryDir d; ObsBackend b(d.path() + "/token");
        for (int corner = 0; corner < 4; ++corner) {
            const QByteArray f = recordWithCamera(b, d.path(), 2, corner, 1);
            QVERIFY2(f.size() == 192 * 108 * 3, qPrintable(QString("no frame for corner %1").arg(corner)));
            const QRect r = Placement::cameraRect(corner, 1);
            QVERIFY2(greenAt(f, r.center().x(), r.center().y()), qPrintable(QString("corner %1: camera not at %2,%3").arg(corner).arg(r.center().x()).arg(r.center().y())));
            // and NOT in the opposite corner, where the screen must show
            const QRect o = Placement::cameraRect(3 - corner, 1);
            QVERIFY2(redAt(f, o.center().x(), o.center().y()), qPrintable(QString("corner %1: opposite corner should be screen").arg(corner)));
            QVERIFY(redAt(f, 960, 540));                     // the middle of the frame is always the screen
        }
    }
    void biggerSizesCoverMoreOfTheScreen() {
        QTemporaryDir d; ObsBackend b(d.path() + "/token");
        int prev = 0;
        for (int size = 0; size < 3; ++size) {
            const QByteArray f = recordWithCamera(b, d.path(), 2, 3, size);
            QVERIFY(f.size() == 192 * 108 * 3);
            const int n = greenPixels(f);
            QVERIFY2(n > prev, qPrintable(QString("size %1 covers %2 px, previous %3").arg(size).arg(n).arg(prev)));
            prev = n;
        }
    }
    void justMyCameraFillsTheFrame() {
        QTemporaryDir d; ObsBackend b(d.path() + "/token");
        const QByteArray f = recordWithCamera(b, d.path(), 3, 3, 1);
        QVERIFY(f.size() == 192 * 108 * 3);
        for (QPoint p : {QPoint(100, 100), QPoint(960, 540), QPoint(1820, 980), QPoint(100, 980), QPoint(1820, 100)})
            QVERIFY2(greenAt(f, p.x(), p.y()), qPrintable(QString("not camera at %1,%2").arg(p.x()).arg(p.y())));
    }
    // A live webcam is an ASYNC source (size known only after its first frame). A solid colour hides scaling bugs, so this
    // uses a video file as the stand-in: a test pattern that has picture content in every corner of a 1280x720 frame.
    void anAsyncCameraSourceFillsTheFrameInCameraOnlyMode() {
        QTemporaryDir d;
        const QString clip = d.path() + "/pattern.mp4";
        QProcess g; g.start("ffmpeg", {"-nostdin", "-v", "error", "-y", "-f", "lavfi", "-i", "testsrc2=s=1280x720:r=30", "-t", "20", "-pix_fmt", "yuv420p", clip});
        QVERIFY(g.waitForFinished(30000) && g.exitCode() == 0);
        qputenv("MOBS_TEST_CAMERA", ("video:" + clip).toUtf8());
        ObsBackend b(d.path() + "/token");
        QSignalSpy started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), failed(&b, &RecorderBackend::failed);
        RecordSpec spec; spec.mic = false; spec.pick = 3; spec.path = d.path() + "/fill.mp4"; spec.camera.path = "test";
        b.begin(spec);
        started.wait(15000);
        QVERIFY2(!started.isEmpty(), "never started");
        QTest::qWait(2500);
        b.end();
        finished.wait(20000);
        QVERIFY(failed.isEmpty() && !finished.isEmpty());
        const QByteArray f = stillRgb(spec.path);
        QVERIFY(f.size() == 192 * 108 * 3);
        // testsrc2 is bright and busy everywhere: every probe point across the 1920x1080 canvas must be non-black
        int dark = 0;
        for (QPoint p : {QPoint(60, 60), QPoint(1860, 60), QPoint(60, 1020), QPoint(1860, 1020), QPoint(960, 540), QPoint(480, 270), QPoint(1440, 810)}) {
            const int i = ((p.y() / 10) * 192 + (p.x() / 10)) * 3;
            if ((unsigned char)f[i] + (unsigned char)f[i + 1] + (unsigned char)f[i + 2] < 30) ++dark;
        }
        QVERIFY2(dark == 0, qPrintable(QString("%1 of 7 probe points are black: the picture does not fill the frame").arg(dark)));
    }
    // ---- the live preview, checked on the pixels of the frames it delivers ----------------------------------------------
    static QImage frameFrom(QSignalSpy &spy) { return spy.isEmpty() ? QImage() : qvariant_cast<QImage>(spy.last().first()); }
    static bool greenIn(const QImage &f, int cx, int cy) {   // canvas coordinates 1920x1080 -> preview 640x360
        const QColor c = f.pixelColor(cx / 3, cy / 3);
        return c.green() > c.red() + 50;
    }
    static bool redIn(const QImage &f, int cx, int cy) {
        const QColor c = f.pixelColor(cx / 3, cy / 3);
        return c.red() > c.green() + 50;
    }
    void thePreviewShowsTheSceneAndFollowsTheWebcamPlacement() {
        qputenv("MOBS_TEST_SOURCE", "colour"); qputenv("MOBS_TEST_COLOUR", "FF0000FF"); qputenv("MOBS_TEST_CAMERA", "colour");
        QTemporaryDir d;
        ObsBackend b(d.path() + "/token");
        QSignalSpy frames(&b, &RecorderBackend::previewFrame);
        RecordSpec spec; spec.mic = false; spec.pick = 2; spec.camera.path = "test"; spec.camera.corner = 3; spec.camera.size = 1;
        b.setPreview(true, spec);
        QTRY_VERIFY_WITH_TIMEOUT(frames.count() >= 5, 8000);                 // frames really arrive, repeatedly
        QImage f = frameFrom(frames);
        QCOMPARE(f.size(), QSize(640, 360));
        QRect br = Placement::cameraRect(3, 1);
        QVERIFY(greenIn(f, br.center().x(), br.center().y()));              // the webcam is bottom-right...
        QVERIFY(redIn(f, 960, 540));                                         // ...and the screen fills the rest
        QVERIFY(redIn(f, 150, 150));
        // tap another corner: the SAME scene is moved, with no restart
        frames.clear();
        spec.camera.corner = 0; spec.camera.size = 2;
        b.setPreview(true, spec);
        QTRY_VERIFY_WITH_TIMEOUT(frames.count() >= 4, 8000);
        f = frameFrom(frames);
        QRect tl = Placement::cameraRect(0, 2);
        QVERIFY(greenIn(f, tl.center().x(), tl.center().y()));
        QVERIFY(redIn(f, br.center().x(), br.center().y()));                 // where it used to be is screen again
        b.setPreview(false, spec);
    }
    void switchingThePreviewOffStopsTheFrames() {
        qputenv("MOBS_TEST_SOURCE", "colour"); qputenv("MOBS_TEST_COLOUR", "FF0000FF");
        QTemporaryDir d;
        ObsBackend b(d.path() + "/token");
        QSignalSpy frames(&b, &RecorderBackend::previewFrame);
        RecordSpec spec; spec.mic = false; spec.pick = 1;
        b.setPreview(true, spec);
        QTRY_VERIFY_WITH_TIMEOUT(frames.count() >= 3, 8000);
        b.setPreview(false, spec);
        QTest::qWait(300);                                                    // anything already queued may land
        frames.clear();
        QTest::qWait(1200);
        QCOMPARE(frames.count(), 0);                                          // then nothing: the picture is really off
    }
    void aRecordingReusesThePreviewSceneAndThePreviewKeepsRunning() {
        qputenv("MOBS_TEST_SOURCE", "colour"); qputenv("MOBS_TEST_COLOUR", "FF0000FF"); qputenv("MOBS_TEST_CAMERA", "colour");
        QTemporaryDir d;
        ObsBackend b(d.path() + "/token");
        QSignalSpy frames(&b, &RecorderBackend::previewFrame), started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), failed(&b, &RecorderBackend::failed);
        RecordSpec spec; spec.mic = false; spec.pick = 2; spec.camera.path = "test"; spec.camera.corner = 1; spec.camera.size = 1;
        b.setPreview(true, spec);
        QTRY_VERIFY_WITH_TIMEOUT(frames.count() >= 3, 8000);
        spec.path = d.path() + "/with-preview.mp4";
        b.begin(spec);
        started.wait(15000);
        QVERIFY2(!started.isEmpty(), "recording never started while previewing");
        frames.clear();
        QTest::qWait(1800);
        QVERIFY2(frames.count() >= 5, "the preview stopped while recording");
        b.end();
        finished.wait(20000);
        QVERIFY(failed.isEmpty() && !finished.isEmpty());
        const QByteArray still = stillRgb(spec.path);                         // the file has the same picture as the preview
        QVERIFY(still.size() == 192 * 108 * 3);
        const QRect tr = Placement::cameraRect(1, 1);
        QVERIFY(greenAt(still, tr.center().x(), tr.center().y()));
        QVERIFY(redAt(still, 960, 540));
        frames.clear();
        QTRY_VERIFY_WITH_TIMEOUT(frames.count() >= 3, 8000);                  // and after stopping, the preview carries on
        b.setPreview(false, spec);
    }
    void aRecordingWithoutAPreviewStillCleansUpTheScene() {
        qputenv("MOBS_TEST_SOURCE", "colour"); qputenv("MOBS_TEST_COLOUR", "FF0000FF"); qunsetenv("MOBS_TEST_CAMERA");
        QTemporaryDir d;
        ObsBackend b(d.path() + "/token");
        for (int i = 0; i < 2; ++i) {                                         // twice: nothing may leak between recordings
            QSignalSpy started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished);
            RecordSpec spec; spec.mic = false; spec.pick = 1; spec.path = d.path() + QString("/p%1.mp4").arg(i);
            b.begin(spec);
            started.wait(15000);
            QVERIFY(!started.isEmpty());
            QTest::qWait(1000);
            b.end();
            finished.wait(20000);
            QVERIFY(!finished.isEmpty() && QFileInfo(spec.path).size() > 1000);
        }
    }
    // ---- the quality setting must really change the file ------------------------------------------------------------
    qint64 recordedSize(ObsBackend &b, const QString &dir, int crf, const QString &clip) {
        qputenv("MOBS_TEST_CAMERA", ("video:" + clip).toUtf8());
        QSignalSpy started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), failed(&b, &RecorderBackend::failed);
        RecordSpec spec; spec.mic = false; spec.pick = 3; spec.camera.path = "test"; spec.crf = crf; spec.path = dir + QString("/q%1.mp4").arg(crf);
        b.begin(spec);
        started.wait(15000);
        if (started.isEmpty()) return -1;
        QTest::qWait(2500);
        b.end();
        finished.wait(20000);
        return (finished.isEmpty() || !failed.isEmpty()) ? -1 : QFileInfo(spec.path).size();
    }
    void aHigherQualitySettingMakesABiggerFileOfTheSamePicture() {
        QTemporaryDir d;
        const QString clip = d.path() + "/busy.mp4";
        QProcess g; g.start("ffmpeg", {"-nostdin", "-v", "error", "-y", "-f", "lavfi", "-i", "testsrc2=s=1280x720:r=30", "-t", "20", "-pix_fmt", "yuv420p", clip});
        QVERIFY(g.waitForFinished(30000) && g.exitCode() == 0);
        ObsBackend b(d.path() + "/token");
        const qint64 small = recordedSize(b, d.path(), 32, clip);                // quality "small"
        const qint64 large = recordedSize(b, d.path(), 16, clip);                // quality "high"
        QVERIFY2(small > 0 && large > 0, "a recording failed");
        QVERIFY2(large > small * 1.3, qPrintable(QString("high quality %1 bytes is not clearly bigger than small %2").arg(large).arg(small)));
    }
    void aCameraThatNeverStartsIsReportedAsACameraProblem() {
        qunsetenv("MOBS_TEST_CAMERA");
        QTemporaryDir d;
        ObsBackend b(d.path() + "/token");
        QSignalSpy failed(&b, &RecorderBackend::failed), started(&b, &RecorderBackend::started);
        RecordSpec spec; spec.mic = false; spec.pick = 3; spec.path = d.path() + "/x.mp4"; spec.camera.path = "/dev/does-not-exist";
        b.begin(spec);
        failed.wait(30000);
        QCOMPARE(failed.count(), 1);
        QVERIFY2(failed.first().first().toString().contains("camera"), qPrintable(failed.first().first().toString()));
        QCOMPARE(started.count(), 0);
        QVERIFY(!QFileInfo(spec.path).exists() || QFileInfo(spec.path).size() < 1000);
    }
    void aTakeBroadcastsToAnRtmpServerAndStillSavesItsFile() {
        AudioFixture fixture; QVERIFY(fixture.valid);
        QTemporaryDir d; const QString recv = d.path() + "/recv.flv", path = d.path() + "/t.mp4";
        RtmpReceiver server(recv, 19351, "test-key-abc");
        ObsBackend b(d.path() + "/token");
        QSignalSpy started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), failed(&b, &RecorderBackend::failed);
        QSignalSpy state(&b, &RecorderBackend::streamState), stats(&b, &RecorderBackend::streamStats);
        RecordSpec spec = silentSpec(1, path); spec.mic = true;
        spec.stream.on = true; spec.stream.url = "rtmp://127.0.0.1:19351/live"; spec.stream.key = "test-key-abc"; spec.stream.kbps = 3000; spec.stream.height = 720;
        b.begin(spec);
        QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(statesOf(state).contains("live"), 10000);
        QTest::qWait(5000);
        b.end();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 25000);
        QVERIFY2(failed.isEmpty(), "backend reported a failure");
        QTRY_VERIFY2_WITH_TIMEOUT(server.p.state() == QProcess::NotRunning, "the server never saw the broadcast end", 15000);   // not waitForFinished(): it says false for a process that already ended
        QCOMPARE(statesOf(state), (QStringList{"connecting", "live", "ended"}));              // each state announced once, in order
        QVERIFY(stats.count() >= 2); QVERIFY2(stats.last()[0].toInt() > 500, qPrintable(QString("kbps %1").arg(stats.last()[0].toInt())));
        // what the server received: H.264 + AAC at the size that was asked for, with a keyframe about every 3 s (X recommends 3)
        QCOMPARE(probe(recv, {"-select_streams", "v:0", "-show_entries", "stream=codec_name,width,height", "-of", "csv=p=0"}), QString("h264,1280,720"));
        QCOMPARE(probe(recv, {"-select_streams", "a:0", "-show_entries", "stream=codec_name", "-of", "csv=p=0"}), QString("aac"));
        QCOMPARE(probe(recv, {"-select_streams", "a:0", "-show_entries", "stream=sample_rate,channels", "-of", "csv=p=0"}), QString("44100,2"));   // X recommends 44.1 kHz stereo
        QCOMPARE(probe(recv, {"-select_streams", "v:0", "-show_entries", "stream=r_frame_rate", "-of", "csv=p=0"}), QString("30/1"));           // and 30 frames a second
        const QStringList keys = probe(recv, {"-select_streams", "v:0", "-skip_frame", "nokey", "-show_entries", "frame=pts_time", "-of", "csv=p=0"}).split('\n', Qt::SkipEmptyParts);
        QVERIFY2(keys.size() >= 2, qPrintable(keys.join(",")));
        QVERIFY2(keys.last().toDouble() - keys.first().toDouble() < 3.6 * (keys.size() - 1), qPrintable("keyframes " + keys.join(",")));
        QVERIFY2(audioRms(recv) > 0.02, qPrintable(QString("rms %1").arg(audioRms(recv))));   // the tone really went out
        // and the saved copy is the full-quality file, unaffected by the broadcast's 720p
        QCOMPARE(probe(path, {"-select_streams", "v:0", "-show_entries", "stream=width,height", "-of", "csv=p=0"}), QString("1920,1080"));
        QVERIFY2(durationOf(path) > 4.0, qPrintable(QString("duration %1").arg(durationOf(path))));
    }
    void aBroadcastToNowhereFailsLoudlyAndTheFileCarriesOn() {
        QTemporaryDir d; const QString path = d.path() + "/t.mp4";
        ObsBackend b(d.path() + "/token");
        QSignalSpy started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), failed(&b, &RecorderBackend::failed), state(&b, &RecorderBackend::streamState);
        RecordSpec spec = silentSpec(1, path);
        spec.stream.on = true; spec.stream.url = "rtmp://127.0.0.1:19352/live"; spec.stream.key = "test-key-abc";   // nothing listens there
        b.begin(spec);
        QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(statesOf(state).contains("failed"), 15000);
        QVERIFY(!statesOf(state).contains("live"));
        QString why; for (const auto &a : state) if (a[0].toString() == "failed") why = a[1].toString();
        QVERIFY2(why.contains("connect"), qPrintable(why));
        QVERIFY2(!why.contains("test-key-abc"), "the stream key must never appear in an error");
        QTest::qWait(2500);                                                   // the recording did not notice
        QVERIFY(failed.isEmpty());
        b.end();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 20000);
        QVERIFY2(durationOf(path) > 2.0, qPrintable(QString("duration %1").arg(durationOf(path))));
    }
    void aBroadcastThatIsNotAskedForCreatesNothing() {
        QTemporaryDir d;
        ObsBackend b(d.path() + "/token");
        QSignalSpy started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), state(&b, &RecorderBackend::streamState);
        b.begin(silentSpec(1, d.path() + "/t.mp4"));
        QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 15000);
        QVERIFY(!obs_get_output_by_name("mobs stream"));
        QTest::qWait(800); b.end();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 20000);
        QVERIFY(state.isEmpty());
    }
    void aHardwareBroadcastEitherGoesLiveOrExplainsItself() {
        QTemporaryDir d; const QString recv = d.path() + "/recv.flv";
        RtmpReceiver server(recv, 19353, "test-key-abc");
        ObsBackend b(d.path() + "/token");
        QSignalSpy checked(&b, &RecorderBackend::hardwareChecked); b.checkHardware(); QCOMPARE(checked.count(), 1);
        if (!checked.first()[0].toBool()) QSKIP("no NVIDIA H.264 on this machine");
        QSignalSpy started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), failed(&b, &RecorderBackend::failed), state(&b, &RecorderBackend::streamState);
        auto spec = silentSpec(1, d.path() + "/gpu.mp4"); spec.hardware = true;
        spec.stream.on = true; spec.stream.url = "rtmp://127.0.0.1:19353/live"; spec.stream.key = "test-key-abc"; spec.stream.kbps = 6000;
        b.begin(spec);
        QTRY_VERIFY_WITH_TIMEOUT(started.count() == 1 || failed.count() == 1, 15000);
        QVERIFY2(failed.isEmpty(), qPrintable(failed.isEmpty() ? QString() : failed.first().first().toString()));
        QTRY_VERIFY_WITH_TIMEOUT(statesOf(state).contains("live"), 10000);
        QTest::qWait(5000); b.end();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 25000);
        QTRY_VERIFY_WITH_TIMEOUT(server.p.state() == QProcess::NotRunning, 15000);
        QCOMPARE(probe(recv, {"-select_streams", "v:0", "-show_entries", "stream=codec_name,width,height", "-of", "csv=p=0"}), QString("h264,1920,1080"));
        const QStringList keys = probe(recv, {"-select_streams", "v:0", "-skip_frame", "nokey", "-show_entries", "frame=pts_time", "-of", "csv=p=0"}).split('\n', Qt::SkipEmptyParts);
        QVERIFY2(keys.size() >= 2 && keys.last().toDouble() - keys.first().toDouble() < 3.6 * (keys.size() - 1), qPrintable("keyframes " + keys.join(",")));
        QVERIFY(durationOf(spec.path) > 4.0);
    }
    void aDroppedConnectionReconnectsAndTheFileNeverNoticed() {
        QTemporaryDir d; const QString path = d.path() + "/t.mp4";
        auto server = std::make_unique<RtmpReceiver>(d.path() + "/one.flv", 19354, "test-key-abc");
        ObsBackend b(d.path() + "/token");
        QSignalSpy started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), failed(&b, &RecorderBackend::failed), state(&b, &RecorderBackend::streamState);
        RecordSpec spec = silentSpec(1, path);
        spec.stream.on = true; spec.stream.url = "rtmp://127.0.0.1:19354/live"; spec.stream.key = "test-key-abc"; spec.stream.kbps = 3000;
        b.begin(spec);
        QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(statesOf(state).contains("live"), 10000);
        QTest::qWait(1500);
        server.reset();                                                       // the "internet" goes away mid-broadcast
        QTRY_VERIFY_WITH_TIMEOUT(statesOf(state).contains("reconnecting"), 20000);
        QVERIFY(failed.isEmpty());                                            // the saved file did not notice
        QString why; for (const auto &a : state) if (a[0].toString() == "reconnecting") why = a[1].toString();
        QVERIFY2(why.contains("trying again"), qPrintable(why));
        auto again = std::make_unique<RtmpReceiver>(d.path() + "/two.flv", 19354, "test-key-abc");   // and comes back
        QTRY_VERIFY_WITH_TIMEOUT(statesOf(state).last() == "live", 40000);
        QTest::qWait(3000);
        b.end();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 25000);
        QVERIFY(failed.isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(again->p.state() == QProcess::NotRunning, 15000);
        QCOMPARE(probe(d.path() + "/two.flv", {"-select_streams", "v:0", "-show_entries", "stream=codec_name", "-of", "csv=p=0"}), QString("h264"));   // video flows again
        QVERIFY2(durationOf(path) > 6.0, qPrintable(QString("duration %1").arg(durationOf(path))));           // the file ran through the whole outage
        QCOMPARE(statesOf(state).last(), QString("ended"));
    }
    // ---- dragging the webcam while a take is recording: the audience (and the file) see it move
    void theWebcamMovesWhileATakeIsRecording() {
        qputenv("MOBS_TEST_COLOUR", "FF0000FF"); qputenv("MOBS_TEST_CAMERA", "colour");     // red screen, green webcam
        QTemporaryDir d; ObsBackend b(d.path() + "/token");
        QSignalSpy started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), failed(&b, &RecorderBackend::failed);
        RecordSpec spec; spec.mic = false; spec.pick = 2; spec.path = d.path() + "/move.mp4"; spec.camera.path = "test"; spec.camera.x = 32; spec.camera.y = 32;   // top-left
        b.begin(spec); started.wait(20000); QVERIFY(!started.isEmpty());
        QTest::qWait(2500);
        RecordSpec moved = spec; moved.camera.x = 1427; moved.camera.y = 789;               // dragged to the bottom-right, mid-take
        b.setPreview(true, moved);
        QTest::qWait(3000);
        b.end(); finished.wait(20000); QVERIFY(failed.isEmpty());
        const auto still = [&](double seconds) {
            QProcess p; p.start("ffmpeg", {"-nostdin", "-v", "error", "-ss", QString::number(seconds), "-i", spec.path, "-frames:v", "1", "-vf", "scale=192:108", "-f", "rawvideo", "-pix_fmt", "rgb24", "-"});
            p.waitForFinished(20000); return p.readAllStandardOutput(); };
        const QByteArray before = still(1.0), after = still(5.0);
        QVERIFY(greenAt(before, 262, 161) && !greenAt(before, 1657, 918));                    // first it sat top-left
        QVERIFY(greenAt(after, 1657, 918) && !greenAt(after, 262, 161));                      // then it was bottom-right, in the SAME file
        QVERIFY(redAt(after, 262, 161));                                                      // and the screen it uncovered is there
    }
    // ---- "the show": the shared window big, the guest's window above, you below, on a quiet backdrop
    QByteArray recordTheShow(const QString &dir, const QString &name, const QList<int> &crop) {
        ObsBackend b(dir + "/token");
        QSignalSpy started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), failed(&b, &RecorderBackend::failed);
        RecordSpec spec; spec.mic = false; spec.pick = 4; spec.path = dir + "/" + name + ".mp4";
        spec.camera.path = "test"; for (int i = 0; i < 4; ++i) spec.camera.guestCrop[i] = crop.value(i);
        b.begin(spec); started.wait(20000);
        if (started.isEmpty()) return {};
        QTest::qWait(2200);                       // long enough for the trim (applied once the guest's size is known) to be in place
        b.end(); finished.wait(20000);
        if (finished.isEmpty() || !failed.isEmpty()) return {};
        return stillRgb(spec.path);
    }
    void theShowPutsTheSharedWindowTheGuestAndMeInTheirTiles() {
        qputenv("MOBS_TEST_COLOUR", "FF0000FF"); qputenv("MOBS_TEST_CAMERA", "colour"); qputenv("MOBS_TEST_GUEST", "colour");   // red window, green you, blue guest
        QTemporaryDir d; const QList<int> none{0, 0, 0, 0};
        const QByteArray f = recordTheShow(d.path(), "show", none);
        QVERIFY(!f.isEmpty());
        QVERIFY(redAt(f, 726, 540));              // the shared window: big, on the left
        QVERIFY(blueAt(f, 1680, 406));            // the guest: top right
        QVERIFY(greenAt(f, 1680, 674));           // you: below the guest
        QVERIFY(darkAt(f, 700, 60)); QVERIFY(darkAt(f, 700, 1010)); QVERIFY(darkAt(f, 1680, 100)); QVERIFY(darkAt(f, 1680, 950));   // the backdrop above and below the pictures (the gaps between them are narrower than a pixel of this still)
        QVERIFY(!redAt(f, 1680, 406) && !redAt(f, 1680, 674));                                                                      // nothing spills into the tiles
        QCOMPARE(probe(d.path() + "/show.mp4", {"-select_streams", "v:0", "-show_entries", "stream=width,height", "-of", "csv=p=0"}), QString("1920,1080"));
    }
    void theGuestsWindowIsTrimmedByPercentageAndNotStretched() {
        qputenv("MOBS_TEST_COLOUR", "FF0000FF"); qputenv("MOBS_TEST_CAMERA", "colour");
        QTemporaryDir d;
        // The guest's "call window": its left half red (somebody else), its right half blue (the guest), 1280x720.
        QProcess make; make.start("ffmpeg", {"-nostdin", "-v", "error", "-f", "lavfi", "-i", "color=c=red:s=640x720:r=15:d=8", "-f", "lavfi", "-i", "color=c=blue:s=640x720:r=15:d=8",
                                             "-filter_complex", "hstack", "-c:v", "libx264", "-pix_fmt", "yuv420p", d.path() + "/call.mp4"});
        QVERIFY(make.waitForFinished(20000) && make.exitCode() == 0);
        qputenv("MOBS_TEST_GUEST", ("video:" + d.path() + "/call.mp4").toUtf8());
        const QRect tile(1452, 278, 456, 256);
        const auto isRed = [](unsigned char r, unsigned char g, unsigned char b) { return r > g + 80 && r > b + 80; };
        const auto isBlue = [](unsigned char r, unsigned char g, unsigned char b) { return b > r + 80 && b > g + 80; };
        const QList<int> none{0, 0, 0, 0};
        const QByteArray whole = recordTheShow(d.path(), "whole", none);                        // control: with no trim the tile shows both halves
        QVERIFY(!whole.isEmpty());
        QVERIFY2(pixelsIn(whole, tile, isRed) > 100 && pixelsIn(whole, tile, isBlue) > 100, "the untrimmed tile should show both halves");
        QVERIFY(redAt(whole, 1452 + 100, 406) && blueAt(whole, 1452 + 360, 406));
        const QList<int> keepRight{50, 0, 0, 0};                                                 // trim the left half away
        const QByteArray trimmed = recordTheShow(d.path(), "trimmed", keepRight);
        QVERIFY(!trimmed.isEmpty());
        QCOMPARE(pixelsIn(trimmed, tile, isRed), 0);                                             // the other person is gone
        QVERIFY2(pixelsIn(trimmed, tile, isBlue) > 400, qPrintable(QString("blue %1").arg(pixelsIn(trimmed, tile, isBlue))));
        QVERIFY(blueAt(trimmed, 1680, 406));
        QVERIFY(darkAt(trimmed, 1452 + 20, 406));                                                // not stretched to fill: bars of backdrop on both sides
    }
    // The live picture must keep flowing while a take is recording, with either encoder: the NVIDIA one reads the scene on the GPU,
    // the picture is a CPU copy of the same scene, and the two must not starve each other.
    int previewFramesDuringATake(bool hardware, int pick) {
        qputenv("MOBS_TEST_CAMERA", "colour");
        QTemporaryDir d; ObsBackend b(d.path() + "/token");          // ONE backend: libobs cannot be started twice in a process
        if (hardware) {
            QSignalSpy checked(&b, &RecorderBackend::hardwareChecked); b.checkHardware();
            if (checked.isEmpty() || !checked.first()[0].toBool()) return -4;   // no NVIDIA H.264 here
        }
        QSignalSpy frames(&b, &RecorderBackend::previewFrame), started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), failed(&b, &RecorderBackend::failed);
        RecordSpec spec; spec.mic = false; spec.pick = pick; spec.path = d.path() + "/t.mp4"; spec.hardware = hardware; spec.camera.path = "test";
        b.setPreview(true, spec);
        for (int waited = 0; frames.count() <= 3 && waited < 8000; waited += 100) QTest::qWait(100);
        if (frames.count() <= 3) return -3;           // the picture never started
        b.begin(spec); started.wait(20000);
        if (started.isEmpty()) return -1;
        QTest::qWait(500); frames.clear();
        QTest::qWait(3000);                         // 15 fps would give about 45 frames
        const int during = frames.count();
        b.end(); finished.wait(20000);
        return failed.isEmpty() ? during : -2;
    }
    void thePictureKeepsFlowingWhileASoftwareTakeRecords() {
        const int n = previewFramesDuringATake(false, 2);
        QVERIFY2(n > 25, qPrintable(QString("frames in 3 s: %1").arg(n)));
    }
    void thePictureKeepsFlowingWhileAnNvidiaTakeRecords() {
        const int n = previewFramesDuringATake(true, 2);
        if (n == -4) QSKIP("no NVIDIA H.264 on this machine");
        QVERIFY2(n > 25, qPrintable(QString("frames in 3 s with NVIDIA: %1 (-3 = the picture never started, -1 = the take never started)").arg(n)));
    }
    // The bitrate X receives must be the one asked for, with either encoder. Busy picture on purpose: a flat one needs almost no bits
    // whatever the setting says, which is how a wrong bitrate once went unseen.
    double receivedKbps(bool hardware, int askedKbps, int port) {
        QTemporaryDir d;
        QProcess make; make.start("ffmpeg", {"-nostdin", "-v", "error", "-f", "lavfi", "-i", "testsrc2=s=1280x720:r=30:d=20,noise=alls=40:allf=t+u", "-c:v", "libx264", "-crf", "10", "-pix_fmt", "yuv420p", d.path() + "/busy.mp4"});
        if (!make.waitForFinished(60000) || make.exitCode() != 0) return -5;
        qputenv("MOBS_TEST_CAMERA", ("video:" + d.path() + "/busy.mp4").toUtf8());
        const QString recv = d.path() + "/recv.flv";
        RtmpReceiver server(recv, port, "test-key-abc");
        ObsBackend b(d.path() + "/token");
        if (hardware) {
            QSignalSpy checked(&b, &RecorderBackend::hardwareChecked); b.checkHardware();
            if (checked.isEmpty() || !checked.first()[0].toBool()) return -4;
        }
        QSignalSpy started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), failed(&b, &RecorderBackend::failed), state(&b, &RecorderBackend::streamState);
        RecordSpec spec = silentSpec(3, d.path() + "/t.mp4"); spec.hardware = hardware; spec.camera.path = "test";
        spec.stream.on = true; spec.stream.url = QString("rtmp://127.0.0.1:%1/live").arg(port); spec.stream.key = "test-key-abc"; spec.stream.kbps = askedKbps;
        b.begin(spec); started.wait(30000);
        if (started.isEmpty()) return -1;
        for (int waited = 0; !statesOf(state).contains("live") && waited < 10000; waited += 100) QTest::qWait(100);
        QTest::qWait(10000);
        b.end(); finished.wait(25000);
        for (int waited = 0; server.p.state() != QProcess::NotRunning && waited < 15000; waited += 100) QTest::qWait(100);
        const double seconds = probe(recv, {"-show_entries", "format=duration", "-of", "csv=p=0"}).toDouble();
        const double bytes = probe(recv, {"-show_entries", "format=size", "-of", "csv=p=0"}).toDouble();
        return seconds > 5 ? bytes * 8 / seconds / 1000 : -3;
    }
    void theBroadcastHoldsItsBitrateWithSoftwareEncoding() {
        const double kbps = receivedKbps(false, 3000, 19360);
        QVERIFY2(kbps > 2300 && kbps < 3800, qPrintable(QString("asked 3000 kbps (+128 audio), the server got %1").arg(kbps)));
    }
    void theBroadcastHoldsItsBitrateWithNvidiaEncoding() {
        const double kbps = receivedKbps(true, 3000, 19361);
        if (kbps == -4) QSKIP("no NVIDIA H.264 on this machine");
        QVERIFY2(kbps > 2300 && kbps < 3800, qPrintable(QString("asked 3000 kbps (+128 audio), the server got %1").arg(kbps)));
    }
    // ---- the webcam as a WINDOW on the screen: libobs captures only the screen, which already contains it
    void inPipModeTheWebcamIsNotPartOfTheCapturedPictureAndNoCameraIsOpened() {
        qputenv("MOBS_TEST_COLOUR", "FF0000FF"); qputenv("MOBS_TEST_CAMERA", "colour");      // red screen; a green webcam that must NOT appear
        QTemporaryDir d; ObsBackend b(d.path() + "/token");
        QSignalSpy started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), failed(&b, &RecorderBackend::failed);
        RecordSpec spec; spec.mic = false; spec.pick = 2; spec.path = d.path() + "/pip.mp4"; spec.camera.path = "test"; spec.camera.pip = true;
        b.begin(spec); started.wait(20000); QVERIFY(!started.isEmpty());
        QVERIFY(!obs_get_source_by_name("camera") && !obs_get_source_by_name("test camera"));   // no camera source exists at all
        QTest::qWait(1500); b.end(); finished.wait(20000); QVERIFY(failed.isEmpty());
        const QByteArray still = stillRgb(spec.path);
        QCOMPARE(greenPixels(still), 0);                                                         // no webcam composited in
        QVERIFY(redAt(still, 262, 161) && redAt(still, 1657, 918) && redAt(still, 960, 540));    // the whole picture is the screen, corners included
    }
    // switching the window mode on or off rebuilds the picture (a camera source appears / goes) without a restart
    void turningTheWebcamWindowOffBringsTheWebcamBackIntoThePicture() {
        qputenv("MOBS_TEST_COLOUR", "FF0000FF"); qputenv("MOBS_TEST_CAMERA", "colour");
        QTemporaryDir d; ObsBackend b(d.path() + "/token");
        RecordSpec spec; spec.mic = false; spec.pick = 2; spec.camera.path = "test"; spec.camera.pip = true;
        QSignalSpy frames(&b, &RecorderBackend::previewFrame);
        b.setPreview(true, spec);
        for (int waited = 0; frames.count() < 3 && waited < 8000; waited += 100) QTest::qWait(100);
        QVERIFY(frames.count() >= 3); QVERIFY(!obs_get_source_by_name("camera") && !obs_get_source_by_name("test camera"));
        spec.camera.pip = false; frames.clear();
        b.setPreview(true, spec);                                                                 // the toggle in EXPERT
        for (int waited = 0; frames.count() < 3 && waited < 8000; waited += 100) QTest::qWait(100);
        QVERIFY(frames.count() >= 3);
        auto *cam = obs_get_source_by_name("test camera"); QVERIFY(cam); obs_source_release(cam);
    }
    // A diagnostic, not a regression test: MOBS_X_PROBE=1 sends MOBS's real stream path to X's own ingest with a key that is DELIBERATELY INVALID
    // (nothing of anyone's account is involved) and prints, with times, every state X puts it through and how long and how much it was connected.
    // It shows what a refusal by X looks like to MOBS. Set MOBS_X_PROBE_HARDWARE=1 to use the NVIDIA encoder.
    void xRefusalLooksLikeThisToMobs() {
        if (qEnvironmentVariable("MOBS_X_PROBE") != "1") QSKIP("diagnostic: set MOBS_X_PROBE=1 to run (talks to X's ingest with a fake key)");
        QTemporaryDir d; ObsBackend b(d.path() + "/token");
        QSignalSpy started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), state(&b, &RecorderBackend::streamState), stats(&b, &RecorderBackend::streamStats);
        RecordSpec spec = silentSpec(1, d.path() + "/probe.mp4"); spec.hardware = qEnvironmentVariable("MOBS_X_PROBE_HARDWARE") == "1";
        spec.stream.on = true; spec.stream.url = qEnvironmentVariable("MOBS_X_PROBE_URL", "rtmp://va.pscp.tv:80/x"); spec.stream.key = "mobs-invalid-probe-key"; spec.stream.kbps = 6000;
        QElapsedTimer clock; clock.start();
        b.begin(spec); started.wait(20000);
        int seen = 0;
        while (clock.elapsed() < 40000) {
            QTest::qWait(250);
            for (; seen < state.count(); ++seen) qInfo().noquote() << QString("PROBE %1 ms  state=%2  %3").arg(clock.elapsed(), 6).arg(state[seen][0].toString(), state[seen][1].toString());
        }
        qInfo().noquote() << QString("PROBE states in order: %1").arg(statesOf(state).join(" > "));
        qInfo().noquote() << QString("PROBE last stats: %1").arg(stats.isEmpty() ? QString("none (never sent)") : QString("%1 kbps, %2 dropped").arg(stats.last()[0].toInt()).arg(stats.last()[1].toInt()));
        b.end(); finished.wait(20000);
    }
    void cancelBeforeStartIsClean() {
        QTemporaryDir d;
        ObsBackend b(d.path() + "/token");
        QSignalSpy finished(&b, &RecorderBackend::finished);
        b.begin(silentSpec(1, d.path() + "/c.mp4"));
        b.end();                                  // immediately, before the first poll
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
        QVERIFY(finished.first().first().toString().isEmpty() || QFileInfo(finished.first().first().toString()).exists());
    }
};
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    ObsBackendTests tests;
    // libobs leaves plugin DSOs loaded at shutdown. NVENC retains CUDA state across that
    // boundary, so repeated startup/shutdown in one process is unsupported. Each scenario
    // matches MOBS: one backend lifetime, with repeated takes explicitly tested inside it.
    if (argc == 1 && qEnvironmentVariable("MOBS_LIBOBS_TEST") == "1" && app.platformName().contains("wayland")) {
        const auto *meta = tests.metaObject(); int count = 0;
        for (int i = meta->methodOffset(); i < meta->methodCount(); ++i) {
            const auto method = meta->method(i); const auto name = method.name();
            if (method.parameterCount() || method.returnType() != QMetaType::Void || name == "initTestCase" || name == "cleanupTestCase") continue;
            QProcess child; child.start(app.applicationFilePath(), {QString::fromLatin1(name)});
            if (!child.waitForFinished(90000)) { child.kill(); child.waitForFinished(2000); qCritical() << "libobs scenario timed out:" << name; return 1; }
            const auto out = child.readAllStandardOutput(), err = child.readAllStandardError();
            std::fwrite(out.constData(), 1, out.size(), stdout); std::fwrite(err.constData(), 1, err.size(), stderr);
            if (child.exitStatus() != QProcess::NormalExit || child.exitCode()) return 1;
            ++count;
        }
        std::printf("%d isolated libobs scenarios passed\n", count); return 0;
    }
    return QTest::qExec(&tests, argc, argv);
}
#include "tst_obsbackend.moc"
