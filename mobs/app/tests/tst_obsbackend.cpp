// Integration test with the REAL libobs. It needs a Wayland session, so it skips itself everywhere else:
//   QT_QPA_PLATFORM=wayland MOBS_LIBOBS_TEST=1 ./tst_obsbackend      (run in the Test Drive VM)
#include "obsbackend.h"
#include "placement.h"
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

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
static int greenPixels(const QByteArray &f) {
    int n = 0;
    for (int i = 0; i + 2 < f.size(); i += 3) if ((unsigned char)f[i + 1] > (unsigned char)f[i] + 50) ++n;
    return n;
}

class ObsBackendTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        if (qEnvironmentVariable("MOBS_LIBOBS_TEST") != "1") QSKIP("set MOBS_LIBOBS_TEST=1 on a Wayland session to run");
        if (!QGuiApplication::platformName().contains("wayland")) QSKIP("needs QT_QPA_PLATFORM=wayland");
        qputenv("MOBS_TEST_SOURCE", "colour");   // no screen picker in a test
    }
    void recordsPausesAndSavesAValidFile() {
        QTemporaryDir d;
        ObsBackend b(d.path() + "/token");
        QSignalSpy started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), failed(&b, &RecorderBackend::failed);
        const QString path = d.path() + "/t.mp4";
        b.begin(RecordSpec{1, path, {}});
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
            b.begin(RecordSpec{1, path, {}});
            QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 15000);
            QTest::qWait(1200);
            b.end();
            QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 20000);
            QVERIFY(QFileInfo(path).size() > 1000);
        }
    }
    // ---- webcam placement, checked on the real pixels of the saved file -------------------------------------------------
    QByteArray recordWithCamera(const QString &dir, int pick, int corner, int size) {
        qputenv("MOBS_TEST_COLOUR", "FF0000FF");           // red "screen"
        qputenv("MOBS_TEST_CAMERA", "colour");              // green "camera"
        ObsBackend b(dir + "/token");
        QSignalSpy started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), failed(&b, &RecorderBackend::failed);
        RecordSpec spec; spec.pick = pick; spec.path = dir + QString("/cam-%1-%2-%3.mp4").arg(pick).arg(corner).arg(size);
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
        QTemporaryDir d;
        for (int corner = 0; corner < 4; ++corner) {
            const QByteArray f = recordWithCamera(d.path(), 2, corner, 1);
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
        QTemporaryDir d;
        int prev = 0;
        for (int size = 0; size < 3; ++size) {
            const QByteArray f = recordWithCamera(d.path(), 2, 3, size);
            QVERIFY(f.size() == 192 * 108 * 3);
            const int n = greenPixels(f);
            QVERIFY2(n > prev, qPrintable(QString("size %1 covers %2 px, previous %3").arg(size).arg(n).arg(prev)));
            prev = n;
        }
    }
    void justMyCameraFillsTheFrame() {
        QTemporaryDir d;
        const QByteArray f = recordWithCamera(d.path(), 3, 3, 1);
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
        RecordSpec spec; spec.pick = 3; spec.path = d.path() + "/fill.mp4"; spec.camera.path = "test";
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
        RecordSpec spec; spec.pick = 2; spec.camera.path = "test"; spec.camera.corner = 3; spec.camera.size = 1;
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
        RecordSpec spec; spec.pick = 1;
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
        RecordSpec spec; spec.pick = 2; spec.camera.path = "test"; spec.camera.corner = 1; spec.camera.size = 1;
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
            RecordSpec spec; spec.pick = 1; spec.path = d.path() + QString("/p%1.mp4").arg(i);
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
    qint64 recordedSize(const QString &dir, int crf, const QString &clip) {
        qputenv("MOBS_TEST_CAMERA", ("video:" + clip).toUtf8());
        ObsBackend b(dir + "/token");
        QSignalSpy started(&b, &RecorderBackend::started), finished(&b, &RecorderBackend::finished), failed(&b, &RecorderBackend::failed);
        RecordSpec spec; spec.pick = 3; spec.camera.path = "test"; spec.crf = crf; spec.path = dir + QString("/q%1.mp4").arg(crf);
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
        const qint64 small = recordedSize(d.path(), 32, clip);                // quality "small"
        const qint64 large = recordedSize(d.path(), 16, clip);                // quality "high"
        QVERIFY2(small > 0 && large > 0, "a recording failed");
        QVERIFY2(large > small * 1.3, qPrintable(QString("high quality %1 bytes is not clearly bigger than small %2").arg(large).arg(small)));
    }
    void aCameraThatNeverStartsIsReportedAsACameraProblem() {
        qunsetenv("MOBS_TEST_CAMERA");
        QTemporaryDir d;
        ObsBackend b(d.path() + "/token");
        QSignalSpy failed(&b, &RecorderBackend::failed), started(&b, &RecorderBackend::started);
        RecordSpec spec; spec.pick = 3; spec.path = d.path() + "/x.mp4"; spec.camera.path = "/dev/does-not-exist";
        b.begin(spec);
        failed.wait(30000);
        QCOMPARE(failed.count(), 1);
        QVERIFY2(failed.first().first().toString().contains("camera"), qPrintable(failed.first().first().toString()));
        QCOMPARE(started.count(), 0);
        QVERIFY(!QFileInfo(spec.path).exists() || QFileInfo(spec.path).size() < 1000);
    }
    void cancelBeforeStartIsClean() {
        QTemporaryDir d;
        ObsBackend b(d.path() + "/token");
        QSignalSpy finished(&b, &RecorderBackend::finished);
        b.begin(RecordSpec{1, d.path() + "/c.mp4", {}});
        b.end();                                  // immediately, before the first poll
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
        QVERIFY(finished.first().first().toString().isEmpty() || QFileInfo(finished.first().first().toString()).exists());
    }
};
QTEST_MAIN(ObsBackendTests)
#include "tst_obsbackend.moc"
