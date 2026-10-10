#include "appstate.h"
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include "placement.h"

class AppStateTests : public QObject {
    Q_OBJECT
private slots:
    void newUserStartsOnChoose() {
        QTemporaryDir d; QSettings s(d.path() + "/m.ini", QSettings::IniFormat);
        AppState a(&s);
        QCOMPARE(a.screen(), 1);
        QCOMPARE(a.pick(), 0);
    }
    void choosingMovesToReadyAndIsRemembered() {
        QTemporaryDir d; const QString f = d.path() + "/m.ini";
        { QSettings s(f, QSettings::IniFormat); AppState a(&s);
          QSignalSpy sc(&a, &AppState::screenChanged);
          a.choose(2);
          QCOMPARE(a.screen(), 2); QCOMPARE(a.pick(), 2); QCOMPARE(a.pickName(), QString("screen + me"));
          QCOMPARE(sc.count(), 1); }
        QSettings s2(f, QSettings::IniFormat); AppState b(&s2);
        QCOMPARE(b.screen(), 2);   // returning user lands on ready
        QCOMPARE(b.pick(), 2);
    }
    void backReturnsToChooseButKeepsThePick() {
        QTemporaryDir d; QSettings s(d.path() + "/m.ini", QSettings::IniFormat);
        AppState a(&s); a.choose(1); a.back();
        QCOMPARE(a.screen(), 1);
        QCOMPARE(a.pick(), 1);
    }
    void ignoresBadPicks() {
        QTemporaryDir d; QSettings s(d.path() + "/m.ini", QSettings::IniFormat);
        AppState a(&s); a.choose(0); a.choose(4);
        QCOMPARE(a.screen(), 1); QCOMPARE(a.pick(), 0);
    }
    void recordingShowsScreenThreeThenReturns() {
        QTemporaryDir d; QSettings s(d.path() + "/m.ini", QSettings::IniFormat);
        AppState a(&s); a.choose(1);
        QSignalSpy sc(&a, &AppState::screenChanged);
        a.setRecording(true);
        QCOMPARE(a.screen(), 3);
        a.setRecording(true);          // no change, no signal
        QCOMPARE(sc.count(), 1);
        a.setRecording(false);
        QCOMPARE(a.screen(), 2);       // back on ready, with the pick kept
        QCOMPARE(a.pick(), 1);
    }
    // ---- camera choice, corner and size ---------------------------------------------------------------------------
    static QList<CameraInfo> twoCameras() {
        return {{"/dev/v4l/by-id/usb-brio-video-index0", "Logitech BRIO", true}, {"/dev/v4l/by-id/usb-hd-video-index0", "HD Webcam", false}};
    }
    void aCameraPickChoosesTheFirstCameraAutomatically() {
        QTemporaryDir d; QSettings s(d.path() + "/m.ini", QSettings::IniFormat);
        AppState a(&s, nullptr, [] { return twoCameras(); });
        QVERIFY(!a.hasCamera());
        a.choose(2);
        QVERIFY(a.hasCamera());
        QCOMPARE(a.cameraName(), QString("Logitech BRIO"));
        QVERIFY(a.cameraSpec().valid());
        QVERIFY(a.cameraSpec().mjpg720);
    }
    void thePlainScreenPickLeavesTheCameraAlone() {
        QTemporaryDir d; QSettings s(d.path() + "/m.ini", QSettings::IniFormat);
        AppState a(&s, nullptr, [] { return twoCameras(); });
        a.choose(1);
        QVERIFY(!a.hasCamera());
    }
    void theCameraChoiceCornerAndSizeAreRemembered() {
        QTemporaryDir d; const QString f = d.path() + "/m.ini";
        { QSettings s(f, QSettings::IniFormat); AppState a(&s, nullptr, [] { return twoCameras(); });
          a.choose(2); a.setCamera(twoCameras()[1].path); a.setCorner(0); a.setSize(2); }
        QSettings s2(f, QSettings::IniFormat); AppState b(&s2, nullptr, [] { return twoCameras(); });
        QCOMPARE(b.cameraName(), QString("HD Webcam"));
        QCOMPARE(b.corner(), 0);
        QCOMPARE(b.size(), 2);
        QVERIFY(!b.cameraSpec().mjpg720);            // per-camera: the HD Webcam list said no
    }
    void unknownCamerasAndOutOfRangeValuesAreIgnored() {
        QTemporaryDir d; QSettings s(d.path() + "/m.ini", QSettings::IniFormat);
        AppState a(&s, nullptr, [] { return twoCameras(); });
        a.choose(2);
        a.setCamera("/dev/video99"); QCOMPARE(a.cameraName(), QString("Logitech BRIO"));
        a.setCorner(7); a.setCorner(-1); QCOMPARE(a.corner(), 3);
        a.setSize(5); QCOMPARE(a.size(), 1);
    }
    void aChosenCameraThatWasUnpluggedIsNotUsed() {
        QTemporaryDir d; QSettings s(d.path() + "/m.ini", QSettings::IniFormat);
        QList<CameraInfo> present = twoCameras();
        AppState a(&s, nullptr, [&] { return present; });
        a.choose(2);
        QVERIFY(a.hasCamera());
        present.removeFirst(); a.refreshCameras();   // the BRIO was unplugged
        QVERIFY(!a.hasCamera());
        QVERIFY(!a.cameraSpec().valid());            // never hand a missing device to the recorder
        a.choose(2);                                 // choosing again picks what is there
        QCOMPARE(a.cameraName(), QString("HD Webcam"));
    }
    void noCamerasAtAllLeavesTheSpecInvalid() {
        QTemporaryDir d; QSettings s(d.path() + "/m.ini", QSettings::IniFormat);
        AppState a(&s, nullptr, [] { return QList<CameraInfo>(); });
        a.choose(2);
        QVERIFY(!a.hasCamera());
        QVERIFY(!a.cameraSpec().valid());
        QVERIFY(a.cameras().isEmpty());
    }
    void thePanelDiagramUsesTheRecordingsRectangle() {
        QTemporaryDir d; QSettings s(d.path() + "/m.ini", QSettings::IniFormat);
        AppState a(&s, nullptr, [] { return twoCameras(); });
        a.setCorner(3); a.setSize(1);
        const QRect r = Placement::cameraRect(3, 1);
        const QRectF n = a.cameraRectNormalized();
        QCOMPARE(n.x() * Placement::kCanvasW, double(r.x()));
        QCOMPARE(n.height() * Placement::kCanvasH, double(r.height()));
    }
    void previewIsOffByDefaultAndRemembered() {
        QTemporaryDir d; const QString f = d.path() + "/m.ini";
        { QSettings s(f, QSettings::IniFormat); AppState a(&s, nullptr, [] { return QList<CameraInfo>(); });
          QVERIFY(!a.preview());                 // off by default: it lights the camera and may open the screen dialog
          QSignalSpy sp(&a, &AppState::previewChanged);
          a.setPreview(true); a.setPreview(true);
          QCOMPARE(sp.count(), 1); }
        QSettings s2(f, QSettings::IniFormat); AppState b(&s2, nullptr, [] { return QList<CameraInfo>(); });
        QVERIFY(b.preview());
    }
    void panelOpensAndCloses() {
        QTemporaryDir d; QSettings s(d.path() + "/m.ini", QSettings::IniFormat);
        AppState a(&s, nullptr, [] { return twoCameras(); });
        QSignalSpy sp(&a, &AppState::cameraPanelChanged);
        a.setCameraPanel(true); a.setCameraPanel(true); a.setCameraPanel(false);
        QCOMPARE(sp.count(), 2);
    }
};
QTEST_MAIN(AppStateTests)
#include "tst_appstate.moc"
