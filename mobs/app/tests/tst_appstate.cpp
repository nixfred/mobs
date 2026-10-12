#include "appstate.h"
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include "placement.h"

class AppStateTests : public QObject {
    Q_OBJECT
private slots:
    void audioShortcutOpensTheRightPageAndCloseResetsIt() {
        QTemporaryDir d; QSettings s(d.path() + "/s.ini", QSettings::IniFormat); AppState a(&s);
        QSignalSpy changed(&a, &AppState::proChanged);
        a.openAudio(); QVERIFY(a.pro()); QCOMPARE(a.expertTab(), 2); QCOMPARE(changed.count(), 1);
        a.setPro(false); QCOMPARE(a.expertTab(), 0);
        a.setPro(true); QCOMPARE(a.expertTab(), 0);
        a.openAudio(); QCOMPARE(a.expertTab(), 2);
    }
    static QList<CameraInfo> oneCamera() { return {{"/dev/v4l/by-id/usb-brio-video-index0", "Logitech BRIO", true}}; }
    void aNewUserWithACameraLandsOnScreenPlusMeWithNothingToChoose() {
        // Fred, 2026-10-10: "All I want is screen and me." No four cards to puzzle over on the first run.
        QTemporaryDir d; QSettings s(d.path() + "/m.ini", QSettings::IniFormat);
        AppState a(&s, nullptr, [] { return oneCamera(); });
        QCOMPARE(a.screen(), 2); QCOMPARE(a.pick(), 2); QCOMPARE(a.pickName(), QString("screen + me"));
        QVERIFY(a.hasCamera()); QVERIFY(a.cameraSpec().valid());
        a.back(); QCOMPARE(a.screen(), 1);             // the picks are still one tap away
    }
    void aNewUserWithoutACameraLandsOnMyScreen() {
        QTemporaryDir d; QSettings s(d.path() + "/m.ini", QSettings::IniFormat);
        AppState a(&s, nullptr, [] { return QList<CameraInfo>(); });
        QCOMPARE(a.screen(), 2); QCOMPARE(a.pick(), 1);
    }
    void choosingMovesToReadyAndIsRemembered() {
        QTemporaryDir d; const QString f = d.path() + "/m.ini";
        { QSettings s(f, QSettings::IniFormat); AppState a(&s, nullptr, [] { return oneCamera(); });
          a.back();                                      // a new user starts on ready: go to the picks first
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
        AppState a(&s, nullptr, [] { return QList<CameraInfo>(); }); a.back(); a.choose(0); a.choose(5); a.choose(-1);   // 4 is the show now
        QCOMPARE(a.screen(), 1); QCOMPARE(a.pick(), 1);   // nothing changed: still the first-run default
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
        QTemporaryDir d; QSettings s(d.path() + "/m.ini", QSettings::IniFormat); s.setValue("pick", 1);   // a returning "my screen" user, no camera chosen yet
        AppState a(&s, nullptr, [] { return twoCameras(); });
        QVERIFY(!a.hasCamera());
        a.choose(2);
        QVERIFY(a.hasCamera());
        QCOMPARE(a.cameraName(), QString("Logitech BRIO"));
        QVERIFY(a.cameraSpec().valid());
        QVERIFY(a.cameraSpec().mjpg720);
    }
    void thePlainScreenPickLeavesTheCameraAlone() {
        QTemporaryDir d; QSettings s(d.path() + "/m.ini", QSettings::IniFormat); s.setValue("pick", 1);
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
        a.setCorner(7); a.setCorner(-1); QCOMPARE(a.corner(), 1);   // the default is top-right
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
    // ---- dragging the webcam
    void theWebcamCanBeDraggedAnywhereInsideThePictureAndOnlyReleaseRemembersIt() {
        QTemporaryDir d; const QString f = d.path() + "/m.ini";
        { QSettings s(f, QSettings::IniFormat); AppState a(&s, nullptr, [] { return twoCameras(); });
          QSignalSpy changed(&a, &AppState::cameraChanged);
          QCOMPARE(a.cameraSpec().x, -1);                                   // not dragged: the corner decides
          a.moveCamera(0.30, 0.40, false);
          QCOMPARE(a.cameraX(), int(std::lround(0.30 * Placement::kCanvasW))); QCOMPARE(a.cameraSpec().y, int(std::lround(0.40 * Placement::kCanvasH)));
          QCOMPARE(changed.count(), 1);
          QVERIFY(!QSettings(f, QSettings::IniFormat).contains("camX"));    // a drag in progress is not written to disk on every move
          a.moveCamera(0.30, 0.40, false); QCOMPARE(changed.count(), 1);    // the same place: silent
          a.moveCamera(-5, 9, false);                                       // far outside: it stays fully inside the picture
          const QRect r = Placement::cameraRectAt(a.cameraX(), a.cameraY(), 1);
          QCOMPARE(a.cameraX(), 0); QCOMPARE(a.cameraY() + r.height(), Placement::kCanvasH);
          a.moveCamera(0.25, 0.10, true);
          const QRectF n = a.cameraRectNormalized(); const QRect want = Placement::cameraRectAt(a.cameraX(), a.cameraY(), 1);
          QCOMPARE(n.x() * Placement::kCanvasW, double(want.x())); QCOMPARE(n.height() * Placement::kCanvasH, double(want.height())); }
        QSettings s(f, QSettings::IniFormat); AppState b(&s, nullptr, [] { return twoCameras(); });
        QCOMPARE(b.cameraX(), int(std::lround(0.25 * Placement::kCanvasW)));  // remembered after the release
    }
    void tappingACornerPutsADraggedWebcamBackOnIt() {
        QTemporaryDir d; const QString f = d.path() + "/m.ini";
        { QSettings s(f, QSettings::IniFormat); AppState a(&s, nullptr, [] { return twoCameras(); });
          a.setCorner(1); a.moveCamera(0.5, 0.5, true); QVERIFY(a.cameraX() >= 0);
          a.setCorner(1);                                                   // the SAME corner it was on: still a way back
          QCOMPARE(a.cameraX(), -1); QCOMPARE(a.cameraSpec().corner, 1);
          const QRect corner = Placement::cameraRect(1, 1); QCOMPARE(a.cameraRectNormalized().x() * Placement::kCanvasW, double(corner.x())); }
        QSettings s(f, QSettings::IniFormat); QVERIFY(!s.contains("camX"));
        AppState b(&s, nullptr, [] { return twoCameras(); }); QCOMPARE(b.cameraX(), -1);
    }
    void aHandEditedWebcamPositionOutsideThePictureIsIgnored() {
        QTemporaryDir d; QSettings s(d.path() + "/m.ini", QSettings::IniFormat); s.setValue("camX", 99999); s.setValue("camY", 5); s.sync();
        AppState a(&s, nullptr, [] { return twoCameras(); }); QCOMPARE(a.cameraX(), -1);
    }
    // ---- the show: a fourth pick, with the guest's window trimmed
    void theShowIsAFourthPickThatNeedsACameraAndIsRemembered() {
        QTemporaryDir d; const QString ini = d.path() + "/m.ini";
        { QSettings s(ini, QSettings::IniFormat); AppState a(&s, nullptr, [] { return twoCameras(); });
          a.choose(4); QCOMPARE(a.pick(), 4); QCOMPARE(a.pickName(), QString("the show")); QCOMPARE(a.screen(), 2);
          QVERIFY(a.hasCamera());                                     // like the other camera picks, it never starts with "no camera" when one exists
          a.choose(5); a.choose(0); QCOMPARE(a.pick(), 4); }
        QSettings s(ini, QSettings::IniFormat); AppState b(&s, nullptr, [] { return twoCameras(); });
        QCOMPARE(b.pick(), 4); QCOMPARE(b.screen(), 2);               // a returning user lands on ready, with the show chosen
    }
    void theShowLayoutHasThreeTilesInsideTheCanvasAndNothingOverlaps() {
        const auto l = Placement::showLayout();
        const QRect canvas(0, 0, Placement::kCanvasW, Placement::kCanvasH);
        for (const QRect &r : {l.main, l.guest, l.me}) QVERIFY2(canvas.contains(r), qPrintable(QString("%1,%2 %3x%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height())));
        QVERIFY(!l.main.intersects(l.guest)); QVERIFY(!l.main.intersects(l.me)); QVERIFY(!l.guest.intersects(l.me));
        QVERIFY(l.main.width() > 2 * l.guest.width());                 // the shared window is the main thing
        QVERIFY(l.guest.top() < l.me.top());                           // the guest above, you below
        QCOMPARE(l.guest.size(), l.me.size()); QVERIFY(qAbs(l.guest.width() * 9 - l.guest.height() * 16) < 16);   // 16:9 (integer rounding)
    }
    void theGuestCropIsClampedRememberedAndNeverCropsTheGuestAway() {
        QTemporaryDir d; const QString ini = d.path() + "/m.ini";
        { QSettings s(ini, QSettings::IniFormat); AppState a(&s, nullptr, [] { return twoCameras(); });
          QSignalSpy changed(&a, &AppState::cameraChanged);
          a.setGuestCrop(0, 15); a.setGuestCrop(0, 15); QCOMPARE(changed.count(), 1);   // a repeat is silent
          a.setGuestCrop(0, 500); QCOMPARE(a.guestCrop()[0].toInt(), 60);
          a.setGuestCrop(2, 80); QCOMPARE(a.guestCrop()[2].toInt(), 30);                  // left 60 + right 30 = 90: the most the pair may take
          a.setGuestCrop(1, -5); QCOMPARE(a.guestCrop()[1].toInt(), 0);
          a.setGuestCrop(9, 10); a.setGuestCrop(-1, 10);                                  // not an edge
          QCOMPARE(a.cameraSpec().guestCrop[0], 60); QCOMPARE(a.cameraSpec().guestCrop[2], 30); }
        QSettings s(ini, QSettings::IniFormat); AppState b(&s, nullptr, [] { return twoCameras(); });
        QCOMPARE(b.guestCrop()[0].toInt(), 60); QCOMPARE(b.guestCrop()[2].toInt(), 30);
        QSettings bad(d.path() + "/bad.ini", QSettings::IniFormat); bad.setValue("guestCropLeft", 60); bad.setValue("guestCropRight", 60); bad.sync();
        AppState c(&bad, nullptr, [] { return twoCameras(); });
        QCOMPARE(c.guestCrop()[0].toInt(), 0); QCOMPARE(c.guestCrop()[2].toInt(), 0);   // a hand-edited file cannot crop the guest away
    }
    void thePanelDiagramForTheShowUsesTheRecordingsRectangles() {
        QTemporaryDir d; QSettings s(d.path() + "/m.ini", QSettings::IniFormat); AppState a(&s, nullptr, [] { return twoCameras(); });
        const auto l = Placement::showLayout(); const auto n = a.showRectsNormalized();
        QCOMPARE(n["guest"].toRectF().x() * Placement::kCanvasW, double(l.guest.x()));
        QCOMPARE(n["me"].toRectF().height() * Placement::kCanvasH, double(l.me.height()));
        QCOMPARE(n["main"].toRectF().width() * Placement::kCanvasW, double(l.main.width()));
    }
    void reviewShowsScreenFourAndRecordingWinsOverIt() {
        QTemporaryDir d; QSettings s(d.path() + "/m.ini", QSettings::IniFormat);
        AppState a(&s, nullptr, [] { return QList<CameraInfo>(); });
        a.choose(1);
        QSignalSpy sc(&a, &AppState::screenChanged);
        a.setReviewing(true);
        QCOMPARE(a.screen(), 4);
        a.setReviewing(true);                                             // no change, no signal
        QCOMPARE(sc.count(), 1);
        a.setRecording(true);                                             // a new recording (e.g. the hotkey) takes the screen
        QCOMPARE(a.screen(), 3);
        a.setRecording(false);
        QCOMPARE(a.screen(), 4);                                          // still reviewing if nobody closed it
        a.setReviewing(false);
        QCOMPARE(a.screen(), 2);
    }
    void previewIsOnByDefaultAndTheChoiceToTurnItOffIsRemembered() {
        QTemporaryDir d; const QString f = d.path() + "/m.ini";
        { QSettings s(f, QSettings::IniFormat); AppState a(&s, nullptr, [] { return QList<CameraInfo>(); });
          QVERIFY(a.preview());                  // on by default (Fred, 2026-10-10: "I still dont see what is being recorded"); the chip turns it off
          QSignalSpy sp(&a, &AppState::previewChanged);
          a.setPreview(false); a.setPreview(false);
          QCOMPARE(sp.count(), 1); }
        QSettings s2(f, QSettings::IniFormat); AppState b(&s2, nullptr, [] { return QList<CameraInfo>(); });
        QVERIFY(!b.preview());
    }
    void theProDrawerOpensAndClosesAndStartsClosed() {
        QTemporaryDir d; QSettings s(d.path() + "/m.ini", QSettings::IniFormat);
        AppState a(&s, nullptr, [] { return QList<CameraInfo>(); });
        QVERIFY(!a.pro());
        QSignalSpy sp(&a, &AppState::proChanged);
        a.setPro(true); a.setPro(true); a.setPro(false);
        QCOMPARE(sp.count(), 2);
    }
    void panelOpensAndCloses() {
        QTemporaryDir d; QSettings s(d.path() + "/m.ini", QSettings::IniFormat);
        AppState a(&s, nullptr, [] { return twoCameras(); });
        QSignalSpy sp(&a, &AppState::cameraPanelChanged);
        a.setCameraPanel(true); a.setCameraPanel(true); a.setCameraPanel(false);
        QCOMPARE(sp.count(), 2);
    }
    // ---- the guided setup ("easy mode")
    void aCameraLockedInIsRememberedAndNeverSwappedForAnotherOne() {
        QTemporaryDir d; const QString path = d.path() + "/lock.ini";
        const auto both = [] { return QList<CameraInfo>{{"/dev/v4l/by-id/usb-brio-video-index0", "Logitech BRIO", true}, {"/dev/v4l/by-id/usb-hp-video-index0", "HP Webcam", false}}; };
        { QSettings s(path, QSettings::IniFormat); AppState a(&s, nullptr, both);
          QVERIFY(!a.cameraLocked());
          a.lockCamera("/dev/v4l/by-id/not-a-camera"); QVERIFY(!a.cameraLocked());       // only a camera that exists can be locked in
          a.lockCamera("/dev/v4l/by-id/usb-hp-video-index0");
          QVERIFY(a.cameraLocked()); QCOMPARE(a.camera(), QString("/dev/v4l/by-id/usb-hp-video-index0")); QCOMPARE(a.lockedCameraName(), QString("HP Webcam")); }
        // next launch with that camera unplugged: it is still the locked one, reported as missing, and the BRIO is NOT quietly used instead
        QSettings s2(path, QSettings::IniFormat);
        AppState b(&s2, nullptr, [] { return QList<CameraInfo>{{"/dev/v4l/by-id/usb-brio-video-index0", "Logitech BRIO", true}}; });
        QVERIFY(b.cameraLocked()); QVERIFY(!b.hasCamera());
        QCOMPARE(b.camera(), QString("/dev/v4l/by-id/usb-hp-video-index0")); QCOMPARE(b.lockedCameraName(), QString("HP Webcam"));
        b.choose(2); b.refreshCameras(); QVERIFY(!b.hasCamera());
        // choosing another camera on purpose moves the lock with it; unlocking lets MOBS pick for itself again
        b.setCamera("/dev/v4l/by-id/usb-brio-video-index0");
        QVERIFY(b.cameraLocked()); QVERIFY(b.hasCamera()); QCOMPARE(b.lockedCameraName(), QString("Logitech BRIO"));
        b.unlockCamera(); QVERIFY(!b.cameraLocked());
    }
    void theSetupGuideAsksForThePictureOnlyWhileAStepNeedsIt() {
        QTemporaryDir d; QSettings s(d.path() + "/setup.ini", QSettings::IniFormat);
        AppState a(&s, nullptr, [] { return twoCameras(); });
        QSignalSpy changed(&a, &AppState::setupChanged);
        QVERIFY(!a.setupOpen()); QVERIFY(a.previewWanted());                  // normal use: the preview chip decides (on by default)
        a.openSetup(); QVERIFY(a.setupOpen()); QVERIFY(!a.previewWanted());   // the guide is up: no webcam window and no screen picture yet
        a.setSetupPreview(true); QVERIFY(a.previewWanted());                  // the screen step needs the picture
        a.closeSetup(); QVERIFY(!a.setupOpen()); QVERIFY(a.previewWanted());  // the guide closed: back to the chip
        a.setPreview(false); QVERIFY(!a.previewWanted());
        QVERIFY(changed.count() >= 3);
    }
    void onlyPagesOfXAreEverOpenedFromTheGuide() {
        qputenv("MOBS_NO_OPEN", "1");                                          // never a real browser from a test
        QTemporaryDir d; QSettings s(d.path() + "/url.ini", QSettings::IniFormat);
        AppState a(&s, nullptr, [] { return QList<CameraInfo>(); });
        QVERIFY(a.openUrl("https://studio.x.com/live")); QVERIFY(a.openUrl("https://x.com/i/live-studio"));
        QVERIFY(!a.openUrl("https://evil.example/x.com")); QVERIFY(!a.openUrl("http://x.com/live")); QVERIFY(!a.openUrl("file:///etc/passwd"));
        QVERIFY(!a.openUrl("https://notx.com/")); QVERIFY(!a.openUrl(""));
    }
};
QTEST_MAIN(AppStateTests)
#include "tst_appstate.moc"
