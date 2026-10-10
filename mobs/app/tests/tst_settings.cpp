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
    // ---- going live
    void theStreamAddressMustBeRtmpAndIsRemembered() {
        QTemporaryDir d; const QString ini = d.path() + "/s.ini";
        { QSettings s(ini, QSettings::IniFormat); Settings c(&s, nullptr, d.path() + "/key");
          QVERIFY(c.streamUrl().isEmpty());
          QVERIFY(!c.setStreamUrl("").isEmpty());
          QVERIFY(c.setStreamUrl("https://x.com/live").contains("rtmp://"));
          QVERIFY(!c.setStreamUrl("rtmp://").isEmpty());
          QVERIFY(!c.setStreamUrl("rtmp://has space/x").isEmpty());
          QVERIFY(c.streamUrl().isEmpty());                                     // nothing bad was kept
          QVERIFY(c.setStreamUrl("  rtmps://live.example:443/x ").isEmpty());
          QCOMPARE(c.streamUrl(), QString("rtmps://live.example:443/x")); }
        QSettings s(ini, QSettings::IniFormat); Settings c(&s, nullptr, d.path() + "/key");
        QCOMPARE(c.streamUrl(), QString("rtmps://live.example:443/x"));
        QSettings bad(d.path() + "/bad.ini", QSettings::IniFormat); bad.setValue("streamUrl", "javascript:alert(1)"); bad.sync();
        QVERIFY(Settings(&bad, nullptr, d.path() + "/key").streamUrl().isEmpty());   // a hand-edited bad value is ignored
    }
    void theStreamKeyLivesInAPrivateFileNeverInTheSettings() {
        QTemporaryDir d; const QString ini = d.path() + "/s.ini", keyFile = d.path() + "/sub/key";
        QSettings s(ini, QSettings::IniFormat); Settings c(&s, nullptr, keyFile);
        QSignalSpy changed(&c, &Settings::changed);
        QVERIFY(!c.streamKeySaved()); QVERIFY(!c.streamReady());
        QVERIFY(c.setStreamKey("  abcd-1234-EFGH \n").isEmpty());
        QVERIFY(c.streamKeySaved()); QVERIFY(changed.count() >= 1);
        const auto mode = QFile::permissions(keyFile);                                      // Qt reports the owner bits twice (Owner and User)
        QVERIFY(mode & QFile::ReadOwner); QVERIFY(mode & QFile::WriteOwner);
        QVERIFY(!(mode & (QFile::ExeOwner | QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup | QFile::ReadOther | QFile::WriteOther | QFile::ExeOther)));   // owner only: 0600
        c.setStreamUrl("rtmp://live.example/x"); s.sync();
        QVERIFY(c.streamReady());
        QCOMPARE(c.streamSpec().key, QString("abcd-1234-EFGH")); QCOMPARE(c.streamSpec().url, QString("rtmp://live.example/x"));
        QFile text(ini); QVERIFY(text.open(QIODevice::ReadOnly)); QVERIFY(!text.readAll().contains("abcd-1234"));   // never in the ini
        QVERIFY(c.property("streamKey").isNull());                                          // and not a property QML can read back
        Settings again(&s, nullptr, keyFile); QVERIFY(again.streamKeySaved());              // remembered across runs
        c.clearStreamKey();
        QVERIFY(!c.streamKeySaved()); QVERIFY(!QFileInfo::exists(keyFile)); QVERIFY(!c.streamReady()); QVERIFY(c.streamSpec().key.isEmpty());
    }
    void aBadStreamKeyIsRefusedAndKeepsTheOldOne() {
        QTemporaryDir d; QSettings s(d.path() + "/s.ini", QSettings::IniFormat); Settings c(&s, nullptr, d.path() + "/key");
        QVERIFY(c.setStreamKey("good-key").isEmpty());
        QVERIFY(!c.setStreamKey("").isEmpty()); QVERIFY(!c.setStreamKey("two words").isEmpty()); QVERIFY(!c.setStreamKey(QString(600, 'a')).isEmpty());
        QCOMPARE(c.streamSpec().key, QString("good-key"));
        Settings nowhere(&s); QVERIFY(!nowhere.setStreamKey("abc").isEmpty());              // no key file path: a test can never write a real one
    }
    void streamQualityPicksTheBitrateAndSize() {
        QTemporaryDir d; const QString ini = d.path() + "/s.ini";
        { QSettings s(ini, QSettings::IniFormat); Settings c(&s, nullptr, d.path() + "/key");
          QCOMPARE(c.streamQuality(), 1); QCOMPARE(c.streamSpec().kbps, 6000); QCOMPARE(c.streamSpec().height, 1080);
          c.setStreamQuality(0); QCOMPARE(c.streamSpec().kbps, 3000); QCOMPARE(c.streamSpec().height, 720);
          c.setStreamQuality(2); QCOMPARE(c.streamSpec().kbps, 9000); QCOMPARE(c.streamSpec().height, 1080);
          c.setStreamQuality(7); QCOMPARE(c.streamQuality(), 2); }
        QSettings s(ini, QSettings::IniFormat); QCOMPARE(Settings(&s, nullptr, d.path() + "/key").streamQuality(), 2);
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
