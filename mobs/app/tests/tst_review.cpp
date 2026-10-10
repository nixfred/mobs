#include "review.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

static bool haveFfmpeg() { return !QStandardPaths::findExecutable("ffmpeg").isEmpty() && !QStandardPaths::findExecutable("ffprobe").isEmpty(); }

// A real take: a test pattern with a tone, `seconds` long.
static bool makeTake(const QString &file, int seconds) {
    QProcess p;
    p.start("ffmpeg", {"-nostdin", "-v", "error", "-y", "-f", "lavfi", "-i", QString("testsrc2=s=320x180:r=15:d=%1").arg(seconds),
                       "-f", "lavfi", "-i", QString("sine=f=440:d=%1").arg(seconds), "-c:v", "libx264", "-pix_fmt", "yuv420p", "-c:a", "aac", "-shortest", file});
    return p.waitForFinished(30000) && p.exitCode() == 0;
}
static double durationOf(const QString &file) {
    QProcess p;
    p.start("ffprobe", {"-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", file});
    p.waitForFinished(10000);
    return p.readAllStandardOutput().trimmed().toDouble();
}
static bool hasAudio(const QString &file) {
    QProcess p;
    p.start("ffprobe", {"-v", "error", "-select_streams", "a", "-show_entries", "stream=codec_type", "-of", "csv=p=0", file});
    p.waitForFinished(10000);
    return p.readAllStandardOutput().contains("audio");
}
static QByteArray hashOf(const QString &file) {
    QFile f(file); f.open(QIODevice::ReadOnly);
    return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha1);
}

class ReviewTests : public QObject {
    Q_OBJECT
    QTemporaryDir dir;
    QString take;
private slots:
    void initTestCase() {
        if (!haveFfmpeg()) QSKIP("ffmpeg/ffprobe not installed");
        take = dir.path() + "/take.mp4";
        QVERIFY(makeTake(take, 6));
    }
    void openReadsDurationAndPauseMarks() {
        QVERIFY(Review::writePauses(take, {1.5, 3.0, 99.0, 0.0}));       // 99 is past the end, 0 is the very start: neither is shown
        Review r;
        QSignalSpy op(&r, &Review::openChanged);
        QVERIFY(r.open(take));
        QVERIFY(r.isOpen());
        QVERIFY(qAbs(r.duration() - 6.0) < 0.3);
        QCOMPARE(r.title(), QString("take"));
        QCOMPARE(r.pauses().size(), 2);
        QCOMPARE(r.clips().size(), 1);                                    // untouched: one clip covering everything
        QVERIFY(!r.changed());
        QVERIFY(op.count() >= 1);
        QFile::remove(Review::sidecarPath(take));
    }
    void aMissingSidecarMeansNoPauses() {
        QFile::remove(Review::sidecarPath(take));
        Review r; QVERIFY(r.open(take));
        QVERIFY(r.pauses().isEmpty());
    }
    void openingGarbageFailsWithAMessageAndStaysClosed() {
        const QString junk = dir.path() + "/junk.mp4";
        QFile f(junk); f.open(QIODevice::WriteOnly); f.write("not a video"); f.close();
        Review r;
        QVERIFY(!r.open(junk));
        QVERIFY(!r.isOpen());
        QVERIFY(r.message().contains("could not open"));
        QVERIFY(!r.open(dir.path() + "/does-not-exist.mp4"));
    }
    void splitCutsAClipIntoTwoAndTinyPiecesAreRefused() {
        Review r; QVERIFY(r.open(take));
        r.split(2.0);
        QCOMPARE(r.clips().size(), 2);
        r.split(2.04);                                                    // 0.04 s from the split: too short to keep
        QCOMPARE(r.clips().size(), 2);
        QVERIFY(!r.changed());                                            // a split alone changes nothing in the output...
        r.removeClip(0);
        QVERIFY(r.changed());                                             // ...cutting a part out does
    }
    void removingAClipLeavesAGapAndTheLastClipCannotBeRemoved() {
        Review r; QVERIFY(r.open(take));
        r.removeClip(0);                                                  // only one clip: refused
        QCOMPARE(r.clips().size(), 1);
        r.split(2.0); r.split(4.0);
        QCOMPARE(r.clips().size(), 3);
        r.removeClip(1);
        QCOMPARE(r.clips().size(), 2);
        QVERIFY(qAbs(r.keptDuration() - (r.duration() - 2.0)) < 0.2);
    }
    void aClipCanGrowIntoAGapButNeverOverItsNeighbour() {
        Review r; QVERIFY(r.open(take));
        r.split(2.0); r.split(4.0); r.removeClip(1);                      // clips: [0,2] and [4,6]; gap 2..4
        r.setClip(0, 0.0, 3.0);                                           // grows into the gap: fine
        QCOMPARE(r.clips()[0].toMap()["end"].toDouble(), 3.0);
        r.setClip(0, 0.0, 5.5);                                           // would overrun clip 2: clamped to its start
        QVERIFY(r.clips()[0].toMap()["end"].toDouble() <= r.clips()[1].toMap()["start"].toDouble() + 1e-9);
    }
    void joiningTwoClipsRestoresTheCut() {
        Review r; QVERIFY(r.open(take));
        r.split(3.0);
        r.joinClips(0);
        QCOMPARE(r.clips().size(), 1);
        QVERIFY(!r.changed());
    }
    void aHandleDragIsOneUndoStep() {
        Review r; QVERIFY(r.open(take));
        r.split(3.0);
        QCOMPARE(r.clips().size(), 2);
        r.beginGesture();
        for (double e = 2.9; e > 2.2; e -= 0.1) r.setClip(0, 0.0, e);     // dozens of tiny moves while dragging
        r.endGesture();
        QVERIFY(r.canUndo());
        r.undo();                                                         // ONE undo takes back the whole drag
        QCOMPARE(r.clips()[0].toMap()["end"].toDouble(), 3.0);
        QCOMPARE(r.clips().size(), 2);
        r.undo();                                                         // the next undo takes back the split
        QCOMPARE(r.clips().size(), 1);
        QVERIFY(!r.canUndo());
    }
    void redoWorksAndANewEditClearsIt() {
        Review r; QVERIFY(r.open(take));
        r.split(2.0);
        r.undo();
        QVERIFY(r.canRedo());
        r.redo();
        QCOMPARE(r.clips().size(), 2);
        r.undo();
        r.split(4.0);                                                     // a different edit: the old redo is gone
        QVERIFY(!r.canRedo());
    }
    void savingAnUntouchedTakeIsRefusedWithAReason() {
        Review r; QVERIFY(r.open(take));
        r.save();
        QVERIFY(!r.saving());
        QVERIFY(r.message().contains("nothing to save"));
    }
    void savedEditContainsOnlyTheKeptPartsAndTheOriginalIsUntouched() {
        const QByteArray before = hashOf(take);
        const QString outDir = dir.path();
        Review r; QVERIFY(r.open(take));
        r.split(2.0); r.split(4.0); r.removeClip(1);                      // keep 0..2 and 4..6: 4 s of 6
        QVERIFY(r.unsaved());
        QSignalSpy saved(&r, &Review::saved);
        double lastProgress = 0; bool sawProgress = false;
        connect(&r, &Review::saveChanged, this, [&] { lastProgress = r.saveProgress(); if (r.saving()) sawProgress = true; });
        r.save();
        QVERIFY(r.saving());
        QTRY_COMPARE_WITH_TIMEOUT(saved.count(), 1, 60000);
        const QString out = saved.first().first().toString();
        QCOMPARE(out, outDir + "/take (edited).mp4");
        QVERIFY(QFileInfo(out).size() > 1000);
        const double d = durationOf(out);
        QVERIFY2(d > 3.6 && d < 4.5, qPrintable(QString("edited duration %1, expected about 4").arg(d)));
        QVERIFY2(hasAudio(out), "the edit lost the audio");
        QCOMPARE(hashOf(take), before);                                   // the ORIGINAL is byte-for-byte unchanged
        QVERIFY(!r.saving());
        QCOMPARE(lastProgress, 1.0);
        QVERIFY(sawProgress);
        QVERIFY(!r.unsaved());                                            // everything is on disk now: leaving is safe
        QVERIFY(r.changed());                                             // but it still differs from the untouched take
        for (const auto &f : QDir(outDir).entryList(QDir::Files | QDir::Hidden))
            QVERIFY2(!f.startsWith(".take (edited)-"), qPrintable("leftover partial file: " + f));
    }
    void aSecondSaveNeverOverwritesTheFirst() {
        Review r; QVERIFY(r.open(take));
        r.split(3.0); r.removeClip(0);
        QSignalSpy saved(&r, &Review::saved);
        r.save();
        QTRY_COMPARE_WITH_TIMEOUT(saved.count(), 1, 60000);
        QCOMPARE(saved.first().first().toString(), dir.path() + "/take (edited) 2.mp4");
        QVERIFY(QFileInfo::exists(dir.path() + "/take (edited).mp4"));    // the first one is still there
    }
    void aFailedExportSaysSoAndLeavesNothingBehind() {
        const QString victim = dir.path() + "/victim.mp4";
        QVERIFY(makeTake(victim, 3));
        Review r; QVERIFY(r.open(victim));
        r.split(1.0); r.removeClip(0);
        QVERIFY(QFile::remove(victim));                                   // the source vanishes before the save runs
        QSignalSpy msg(&r, &Review::saveChanged);
        r.save();
        QTRY_VERIFY_WITH_TIMEOUT(!r.saving() && r.message().contains("could not save"), 30000);
        QVERIFY(r.savedPath().isEmpty());
        QVERIFY(r.unsaved());                                             // still unsaved: the user must not think it worked
        for (const auto &f : QDir(dir.path()).entryList(QDir::Files | QDir::Hidden))
            QVERIFY2(!f.startsWith(".victim (edited)-"), qPrintable("leftover partial file: " + f));
    }
    void closeForgetsTheTake() {
        Review r; QVERIFY(r.open(take));
        r.split(2.0);
        r.close();
        QVERIFY(!r.isOpen());
        QVERIFY(r.clips().isEmpty());
        QVERIFY(r.open(take));
        QCOMPARE(r.clips().size(), 1);                                    // a fresh start, not the old edits
    }
    void deleteMovesToTheTrashAndNeverRemovesDirectly() {
        QTemporaryDir fakeHome;                                           // keep the REAL trash out of the test
        qputenv("XDG_DATA_HOME", fakeHome.path().toUtf8());
        const QString doomed = dir.path() + "/doomed.mp4";
        QVERIFY(makeTake(doomed, 2));
        QVERIFY(Review::writePauses(doomed, {0.5}));
        Review r; QVERIFY(r.open(doomed));
        QVERIFY(r.deleteTake());
        QVERIFY(!QFileInfo::exists(doomed));
        QVERIFY(!QFileInfo::exists(Review::sidecarPath(doomed)));
        QVERIFY(!r.isOpen());
        QVERIFY2(QFileInfo::exists(fakeHome.path() + "/Trash/files/doomed.mp4"), "the take is not in the trash");   // recoverable
        qunsetenv("XDG_DATA_HOME");
    }
    void pausePointsRoundTripThroughTheSidecar() {
        const QString f = dir.path() + "/rt.mp4";
        QVERIFY(Review::writePauses(f, {1.25, 2.5}));
        QCOMPARE(Review::readPauses(f), (QList<double>{1.25, 2.5}));
        QCOMPARE(Review::sidecarPath(f), f + ".mobs.json");
    }
    void editedNamesNeverCollide() {
        const QString f = dir.path() + "/names.mp4";
        QCOMPARE(Review::editedPathFor(f), dir.path() + "/names (edited).mp4");
        QFile a(dir.path() + "/names (edited).mp4"); a.open(QIODevice::WriteOnly); a.close();
        QCOMPARE(Review::editedPathFor(f), dir.path() + "/names (edited) 2.mp4");
    }
};
QTEST_MAIN(ReviewTests)
#include "tst_review.moc"
