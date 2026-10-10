#include "settings.h"
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class SettingsTests : public QObject {
    Q_OBJECT
private slots:
    void defaultsAreBalancedMicOnReviewOnDefaultFolder() {
        QTemporaryDir d; QSettings s(d.path() + "/s.ini", QSettings::IniFormat);
        Settings c(&s);
        QCOMPARE(c.quality(), 1);
        QVERIFY(c.micOn());
        QVERIFY(c.reviewAfter());
        QVERIFY(c.folderIsDefault());
        QCOMPARE(c.folder(), Settings::defaultFolder());
        QVERIFY(c.folder().endsWith("/mobs"));
    }
    void everythingIsRemembered() {
        QTemporaryDir d; const QString f = d.path() + "/s.ini"; const QString out = d.path() + "/takes";
        { QSettings s(f, QSettings::IniFormat); Settings c(&s);
          c.setQuality(2); c.setMicOn(false); c.setReviewAfter(false); c.setFolder(out); }
        QSettings s2(f, QSettings::IniFormat); Settings c(&s2);
        QCOMPARE(c.quality(), 2);
        QVERIFY(!c.micOn());
        QVERIFY(!c.reviewAfter());
        QCOMPARE(c.folder(), out);
        QVERIFY(!c.folderIsDefault());
        QVERIFY(QFileInfo(out).isDir());                                  // choosing a folder creates it
    }
    void qualityMapsToSensibleCrfValuesAndIsClamped() {
        QCOMPARE(Settings::crfFor(0), 28);
        QCOMPARE(Settings::crfFor(1), 23);
        QCOMPARE(Settings::crfFor(2), 18);
        QVERIFY(Settings::crfFor(0) > Settings::crfFor(1) && Settings::crfFor(1) > Settings::crfFor(2));   // better = lower
        QCOMPARE(Settings::crfFor(-5), 28);
        QCOMPARE(Settings::crfFor(99), 18);
    }
    void badQualityValuesAreIgnored() {
        QTemporaryDir d; QSettings s(d.path() + "/s.ini", QSettings::IniFormat);
        Settings c(&s);
        c.setQuality(7); c.setQuality(-1);
        QCOMPARE(c.quality(), 1);
        QSettings corrupt(d.path() + "/c.ini", QSettings::IniFormat); corrupt.setValue("quality", 42); corrupt.sync();
        Settings c2(&corrupt);
        QCOMPARE(c2.quality(), 2);                                        // a hand-edited file cannot give an invalid level
    }
    void aFolderThatCannotBeUsedIsNeverSaved() {
        QTemporaryDir d; QSettings s(d.path() + "/s.ini", QSettings::IniFormat);
        Settings c(&s);
        const QString before = c.folder();
        c.setFolder("relative/path");                                      // not absolute
        QCOMPARE(c.folder(), before);
        c.setFolder("/proc/definitely/not/writable");
        QCOMPARE(c.folder(), before);
        QVERIFY(c.folderIsDefault());
    }
    void emptyFolderGoesBackToTheDefault() {
        QTemporaryDir d; QSettings s(d.path() + "/s.ini", QSettings::IniFormat);
        Settings c(&s);
        c.setFolder(d.path() + "/custom");
        QVERIFY(!c.folderIsDefault());
        c.setFolder("");
        QVERIFY(c.folderIsDefault());
        QCOMPARE(c.folder(), Settings::defaultFolder());
    }
    void problemWithExplainsInWords() {
        QVERIFY(Settings::problemWith("").contains("empty"));
        QVERIFY(Settings::problemWith("rel").contains("full path"));
        QVERIFY(Settings::problemWith("/proc/x/y").contains("create") || Settings::problemWith("/proc/x/y").contains("write"));
        QTemporaryDir d;
        QVERIFY(Settings::problemWith(d.path() + "/new/nested").isEmpty());   // created on the way
    }
    void changesAreSignalledOnceAndNoOpsAreSilent() {
        QTemporaryDir d; QSettings s(d.path() + "/s.ini", QSettings::IniFormat);
        Settings c(&s);
        QSignalSpy sp(&c, &Settings::changed);
        c.setQuality(2); c.setQuality(2);
        c.setMicOn(true);                                                  // already true
        c.setReviewAfter(false);
        QCOMPARE(sp.count(), 2);
    }
    void aboutNamesTheEngineAndTheLicence() {
        QTemporaryDir d; QSettings s(d.path() + "/s.ini", QSettings::IniFormat);
        Settings c(&s);
        QVERIFY(c.aboutText().contains("libobs"));
        QVERIFY(c.aboutText().contains("GPLv2"));
        QVERIFY(c.aboutText().contains("Monologue"));
    }
};
QTEST_MAIN(SettingsTests)
#include "tst_settings.moc"
