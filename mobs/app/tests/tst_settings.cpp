#include "settings.h"
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class SettingsTests : public QObject {
    Q_OBJECT
private slots:
    void audioChoicesPersistAndInvalidGainsAreIgnored() {
        QTemporaryDir d; const QString path = d.path() + "/audio.ini";
        { QSettings s(path, QSettings::IniFormat); Settings c(&s);
          QCOMPARE(c.micDevice(), QString("default")); QCOMPARE(c.micGainDb(), 0);
          QCOMPARE(c.desktopGainDb(), -6); QVERIFY(!c.desktopOn()); QVERIFY(!c.hardware());
          c.setMicDevice("usb"); c.setMicGainDb(-9); c.setDesktopOn(true);
          c.setDesktopDevice("speakers.monitor"); c.setDesktopGainDb(3); c.setHardware(true);
          c.setMicGainDb(-61); c.setDesktopGainDb(13); c.setMicDevice(""); }
        QSettings s(path, QSettings::IniFormat); Settings c(&s);
        QCOMPARE(c.micDevice(), QString("usb")); QCOMPARE(c.micGainDb(), -9);
        QCOMPARE(c.desktopDevice(), QString("speakers.monitor")); QCOMPARE(c.desktopGainDb(), 3);
        QVERIFY(c.desktopOn()); QVERIFY(c.hardware());
        const auto a = c.audioSpec(); QCOMPARE(a.micDevice, c.micDevice()); QVERIFY(a.desktop);
        c.setMicGainDb(-60); QCOMPARE(c.micGainDb(), -60);
        c.setDesktopGainDb(12); QCOMPARE(c.desktopGainDb(), 12);
    }
    void customEncodingPersistsAndQualityPresetsClearIt() {
        QTemporaryDir d; const QString path = d.path() + "/s.ini";
        { QSettings s(path, QSettings::IniFormat); Settings c(&s);
          QCOMPARE(c.crf(), 23); QCOMPARE(c.encoderPreset(), QString("veryfast"));
          c.setCrf(17); c.setEncoderPreset("fast"); }
        QSettings s(path, QSettings::IniFormat); Settings c(&s);
        QCOMPARE(c.crf(), 17); QVERIFY(c.customQuality()); QCOMPARE(c.encoderPreset(), QString("fast"));
        c.setQuality(1); QCOMPARE(c.crf(), 23); QVERIFY(!c.customQuality());
        c.setCrf(0); QCOMPARE(c.crf(), 0); c.setCrf(51); QCOMPARE(c.crf(), 51);
        c.setCrf(52); c.setCrf(-2); QCOMPARE(c.crf(), 51);
        c.setEncoderPreset("unsafe"); QCOMPARE(c.encoderPreset(), QString("fast"));
        c.resetEncoding(); QCOMPARE(c.crf(), 23); QCOMPARE(c.encoderPreset(), QString("veryfast"));
        Settings again(&s); QCOMPARE(again.crf(), 23); QVERIFY(!again.customQuality());
    }
    void corruptedExpertSettingsFallBackToDefaults() {
        QTemporaryDir d; QSettings s(d.path() + "/s.ini", QSettings::IniFormat);
        s.setValue("micGainDb", 999); s.setValue("desktopGainDb", "bad");
        s.setValue("micDevice", ""); s.setValue("desktopDevice", "");
        s.setValue("customCrf", "broken"); s.setValue("encoderPreset", "bad");
        Settings c(&s); QCOMPARE(c.micGainDb(), 0); QCOMPARE(c.desktopGainDb(), -6);
        QCOMPARE(c.micDevice(), QString("default")); QCOMPARE(c.desktopDevice(), QString("default"));
        QCOMPARE(c.crf(), 23); QVERIFY(!c.customQuality());
        QCOMPARE(c.encoderPreset(), QString("veryfast"));
    }
    void defaultsAreBalancedMicOnReviewOnDefaultFolder() {
        QTemporaryDir d; QSettings s(d.path() + "/s.ini", QSettings::IniFormat);
        Settings c(&s);
        QCOMPARE(c.quality(), 1);
        QVERIFY(c.micOn());
        QVERIFY(c.reviewAfter());
        QVERIFY(c.splash());                                              // the opening animation is on until someone turns it off
        QVERIFY(c.folderIsDefault());
        QCOMPARE(c.folder(), Settings::defaultFolder());
        QVERIFY(c.folder().endsWith("/mobs"));
    }
    void everythingIsRemembered() {
        QTemporaryDir d; const QString f = d.path() + "/s.ini"; const QString out = d.path() + "/takes";
        { QSettings s(f, QSettings::IniFormat); Settings c(&s);
          c.setQuality(2); c.setMicOn(false); c.setReviewAfter(false); c.setSplash(false); c.setFolder(out); }
        QSettings s2(f, QSettings::IniFormat); Settings c(&s2);
        QCOMPARE(c.quality(), 2);
        QVERIFY(!c.micOn());
        QVERIFY(!c.reviewAfter());
        QVERIFY(!c.splash());
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
