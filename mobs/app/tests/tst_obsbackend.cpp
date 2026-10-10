// Integration test with the REAL libobs. It needs a Wayland session, so it skips itself everywhere else:
//   QT_QPA_PLATFORM=wayland MOBS_LIBOBS_TEST=1 ./tst_obsbackend      (run in the Test Drive VM)
#include "obsbackend.h"
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
        b.begin(1, path);
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
            b.begin(1, path);
            QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 15000);
            QTest::qWait(1200);
            b.end();
            QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 20000);
            QVERIFY(QFileInfo(path).size() > 1000);
        }
    }
    void cancelBeforeStartIsClean() {
        QTemporaryDir d;
        ObsBackend b(d.path() + "/token");
        QSignalSpy finished(&b, &RecorderBackend::finished);
        b.begin(1, d.path() + "/c.mp4");
        b.end();                                  // immediately, before the first poll
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
        QVERIFY(finished.first().first().toString().isEmpty() || QFileInfo(finished.first().first().toString()).exists());
    }
};
QTEST_MAIN(ObsBackendTests)
#include "tst_obsbackend.moc"
