#include "mobslog.h"
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

class MobsLogTests : public QObject {
    Q_OBJECT
    QTemporaryDir dir;
    QString file() const { return dir.path() + "/state/mobs.log"; }
    static QString read(const QString &path) { QFile f(path); return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString(); }
private slots:
    void initTestCase() {
        QVERIFY(dir.isValid());
        QDir().mkpath(dir.path() + "/state");
        QFile big(file()); QVERIFY(big.open(QIODevice::WriteOnly)); big.write(QByteArray(5 * 1024 * 1024, 'x')); big.close();   // an old, oversized log
        qputenv("MOBS_LOG_FILE", file().toUtf8());
        MobsLog::init();
    }
    void aLogFileExistsAfterStartAndOnlyTheOwnerMayReadIt() {
        QCOMPARE(MobsLog::path(), file());
        const auto mode = QFile::permissions(file());
        QVERIFY(mode & QFile::ReadOwner); QVERIFY(mode & QFile::WriteOwner);
        QVERIFY(!(mode & (QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther)));
    }
    void anOversizedLogIsRotatedNotLostOrGrownForever() {
        QVERIFY(QFileInfo::exists(file() + ".1"));
        QVERIFY(QFileInfo(file() + ".1").size() > 4 * 1024 * 1024);          // the old one is kept
        QVERIFY(QFileInfo(file()).size() < 100 * 1024);                      // the new one starts small
    }
    void linesCarryATimeAndACategory() {
        MobsLog::write("engine", "state idle -> starting");
        const QString text = read(file());
        QVERIFY(text.contains(QRegularExpression(R"(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d{3} \[engine\] state idle -> starting)")));
    }
    void aRegisteredSecretNeverReachesTheFileWhoeverPrintsIt() {
        MobsLog::secret("super-secret-key-9");
        MobsLog::write("stream", "connecting rtmp://host/app/super-secret-key-9 now");
        qWarning("libobs said super-secret-key-9");                          // even through Qt's own messages
        MobsLog::secret({});
        const QString text = read(file());
        QVERIFY(!text.contains("super-secret-key-9"));
        QVERIFY(text.contains("connecting rtmp://host/app/*** now"));
    }
    void everyOtherCopyOfALineIsScrubbedToo() {   // libobs's and Qt's lines also go to stderr, which is the journal under a launcher
        MobsLog::secret("super-secret-key-9");
        QCOMPARE(MobsLog::scrub("libobs: could not reach rtmp://host/app/super-secret-key-9"), QString("libobs: could not reach rtmp://host/app/***"));
        QCOMPARE(MobsLog::scrub("nothing to hide here"), QString("nothing to hide here"));
        MobsLog::secret({});
        QCOMPARE(MobsLog::scrub("super-secret-key-9 is no secret once it is cleared"), QString("super-secret-key-9 is no secret once it is cleared"));
    }
    void qtWarningsAreKeptToo() {
        qWarning("a warning from somewhere");
        QVERIFY(read(file()).contains("[qt] warning: a warning from somewhere"));
    }
    void multilineTextStaysOnOneLine() {
        MobsLog::write("start", "line one\nline two");
        QVERIFY(read(file()).contains("line one line two"));
    }
};
QTEST_MAIN(MobsLogTests)
#include "tst_mobslog.moc"
