#include "audiodevices.h"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class AudioDeviceTests : public QObject {
    Q_OBJECT
private slots:
    void init() { qunsetenv("MOBS_FAKE_AUDIO"); }
    void parsesBothPulseSchemasAndIgnoresDuplicates() {
        QVariantList mics, desktops;
        QVERIFY(AudioDevices::parse(R"json([
          {"name":"mic","description":"USB mic","monitor_of_sink":4294967295},
          {"name":"mic","description":"duplicate"},
          {"name":"another","description":"(null)","monitor_of_sink":"4294967295"},
          {"name":"old-monitor","description":"Speakers","monitor_of_sink":0},
          {"name":"new-monitor","monitor_source":"sink.name"},
          {"name":"output.monitor","monitor_source":null},
          {"name":"plain","monitor_source":"n/a"}, {}, null])json", &mics, &desktops));
        QCOMPARE(mics.size(), 3); QCOMPARE(desktops.size(), 3);
        QCOMPARE(mics[0].toMap()["name"].toString(), QString("USB mic"));
        QCOMPARE(mics[1].toMap()["name"].toString(), QString("another"));
        QVERIFY(!AudioDevices::parse("{}", &mics, &desktops));
        QVERIFY(!AudioDevices::parse("broken", &mics, &desktops));
    }
    void selectedDevicesAreRequiredOnlyWhenEnabled() {
        qputenv("MOBS_FAKE_AUDIO", "1"); AudioDevices devices; devices.refresh();
        QVERIFY(devices.ready()); AudioSpec spec;
        QVERIFY(devices.problem(spec, true).isEmpty());
        spec.micDevice = "missing"; QVERIFY(devices.problem(spec, true).contains("microphone"));
        QVERIFY(devices.problem(spec, false).isEmpty());
        spec.desktop = true; spec.desktopDevice = "missing";
        QVERIFY(devices.problem(spec, false).contains("desktop"));
        spec.desktopDevice = "test:speakers.monitor"; QVERIFY(devices.problem(spec, false).isEmpty());
        QCOMPARE(devices.name("test:usb"), QString("USB microphone"));
        QCOMPARE(devices.name("missing"), QString("unavailable microphone"));
    }
    void asynchronousFailureClearsOldDevicesAndFinishesScanning() {
        qputenv("MOBS_FAKE_AUDIO", "1"); AudioDevices devices(nullptr, "/no/such/pactl"); devices.refresh();
        QVERIFY(devices.ready()); qunsetenv("MOBS_FAKE_AUDIO"); devices.refresh();
        QTRY_VERIFY(!devices.scanning()); QTRY_VERIFY(!devices.error().isEmpty());
        QVERIFY(!devices.ready()); QVERIFY(devices.microphones().isEmpty());
        AudioSpec spec; QVERIFY(!devices.problem(spec, true).isEmpty());
        QVERIFY(devices.problem(spec, false).isEmpty());
    }
    void malformedServiceResponseDoesNotCountAsSuccess() {
        QTemporaryDir dir; const QString path = dir.path() + "/pactl";
        QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("#!/bin/sh\nprintf 'broken'\n"); f.close();
        QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        AudioDevices devices(nullptr, path); devices.refresh();
        QTRY_VERIFY(!devices.error().isEmpty()); QVERIFY(!devices.ready()); QVERIFY(!devices.scanning());
    }
    void unresponsiveServiceTimesOut() {
        QTemporaryDir dir; const QString path = dir.path() + "/pactl";
        QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("#!/bin/sh\nexec sleep 10\n"); f.close();
        QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        AudioDevices devices(nullptr, path); devices.refresh(); devices.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(devices.error().contains("timed out"), 6000);
        QTRY_VERIFY(!devices.scanning()); QVERIFY(!devices.ready());
    }
};
QTEST_MAIN(AudioDeviceTests)
#include "tst_audiodevices.moc"
