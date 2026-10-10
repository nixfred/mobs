#include "appstate.h"
#include "engine.h"
#include "pipcontroller.h"
#include "settings.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

class FakeBackend : public RecorderBackend {
public:
    void begin(const RecordSpec &) override {}
    void pause(bool) override {}
    void end() override {}
    void setPreview(bool, const RecordSpec &) override {}
};

static QJsonArray clients() {
    return QJsonDocument::fromJson(R"([
      {"address":"0xaaa","title":"mobs webcam","pid":111,"floating":false,"pinned":false,"monitor":1,"at":[10,10],"size":[480,270]},
      {"address":"0xbbb","title":"mobs webcam","pid":222,"floating":true,"pinned":true,"monitor":0,"at":[0,0],"size":[480,270]},
      {"address":"0xccc","title":"mobs","pid":111,"floating":false,"pinned":false,"monitor":1,"at":[0,0],"size":[1100,700]}])").array();
}
static QJsonArray monitors() {
    return QJsonDocument::fromJson(R"([
      {"id":0,"name":"DP-1","x":0,"y":0,"width":1080,"height":1920,"scale":1.0,"reserved":[0,0,0,0],"focused":false},
      {"id":1,"name":"eDP-1","x":1080,"y":0,"width":1920,"height":1080,"scale":1.0,"reserved":[0,40,0,0],"focused":true}])").array();
}

class PipControllerTests : public QObject {
    Q_OBJECT
    QTemporaryDir dir;
    static QList<CameraInfo> oneCamera() { return {{"/dev/v4l/by-id/usb-brio-video-index0", "Logitech BRIO", true}}; }
private slots:
    // ---- finding OUR window in `hyprctl clients -j`
    void ourWindowIsFoundByTitleAndProcessIdNeverSomeoneElses() {
        QCOMPARE(PipController::addressOf(clients(), "mobs webcam", 111), QString("0xaaa"));
        QCOMPARE(PipController::addressOf(clients(), "mobs webcam", 222), QString("0xbbb"));   // another MOBS's window is not ours
        QCOMPARE(PipController::addressOf(clients(), "mobs webcam", 333), QString());           // a title alone is not enough
        QCOMPARE(PipController::addressOf(clients(), "something else", 111), QString());
        QCOMPARE(PipController::addressOf({}, "mobs webcam", 111), QString());
    }
    void aClientAndAMonitorAreLookedUpByAddressIdAndName() {
        QCOMPARE(PipController::clientAt(clients(), "0xccc").value("title").toString(), QString("mobs"));
        QVERIFY(PipController::clientAt(clients(), "0xnope").isEmpty()); QVERIFY(PipController::clientAt(clients(), "").isEmpty());
        QCOMPARE(PipController::monitorById(monitors(), 1).value("name").toString(), QString("eDP-1"));
        QCOMPARE(PipController::monitorByName(monitors(), "DP-1").value("id").toInt(), 0);
        QVERIFY(PipController::monitorByName(monitors(), "").isEmpty()); QVERIFY(PipController::monitorById(monitors(), 9).isEmpty());
    }
    // ---- placement
    void theDefaultPlaceIsTheTopRightOfTheUsableAreaBelowTheBar() {
        const QRect r = PipController::defaultRect(PipController::monitorById(monitors(), 1), QSize(480, 270), 24);
        QCOMPARE(r, QRect(1080 + 1920 - 24 - 480, 40 + 24, 480, 270));       // x counts from the monitor's own x; y clears the 40 px bar
    }
    void aScaledMonitorIsMeasuredInLayoutUnitsNotPixels() {
        QJsonObject m{{"id", 0}, {"name", "X"}, {"x", 0}, {"y", 0}, {"width", 3840}, {"height", 2160}, {"scale", 2.0}, {"reserved", QJsonArray{0, 0, 0, 0}}};
        QCOMPARE(PipController::defaultRect(m, QSize(480, 270), 24), QRect(1920 - 24 - 480, 24, 480, 270));
    }
    void aRememberedPlaceIsKeptWhenItStillFitsAndPulledBackInWhenItDoesNot() {
        const QJsonObject m = PipController::monitorById(monitors(), 1);
        QCOMPARE(PipController::fitInside(QRect(1500, 300, 480, 270), m), QRect(1500, 300, 480, 270));
        QCOMPARE(PipController::fitInside(QRect(2900, 900, 480, 270), m), QRect(1080 + 1920 - 480, 40 + (1080 - 40) - 270, 480, 270));   // hanging off the corner: pulled inside
        QVERIFY(PipController::fitInside(QRect(100, 100, 480, 270), m).isNull());          // that place is on ANOTHER monitor: not valid here
    }
    // ---- when the window is wanted
    void theWindowIsWantedExactlyWhenScreenPlusMeShowsItsPicture() {
        QSettings s(dir.path() + "/a.ini", QSettings::IniFormat); Settings prefs(&s); AppState st(&s, nullptr, [] { return oneCamera(); });
        FakeBackend b; Engine e(&b, dir.path()); e.setCameraProvider([&] { auto c = st.cameraSpec(); c.pip = prefs.pip(); return c; });
        PipController pip(&st, &e, &prefs, &s);
        QCOMPARE(st.pick(), 2); QVERIFY(prefs.pip());                       // a new user: screen + me, the window on
        QVERIFY(!pip.wanted());                                             // nothing showing yet
        e.setPreview(true, 2); QVERIFY(pip.wanted());                       // the ready screen's picture is on: the window shows
        prefs.setPip(false); QVERIFY(!pip.wanted()); prefs.setPip(true);    // the toggle in EXPERT
        e.setPreview(false, 2); QVERIFY(!pip.wanted());                     // preview off and no take: gone
        e.start(2); emit b.started(); QVERIFY(pip.wanted());               // a take in progress always shows it, whatever the preview chip says
    }
    void otherPicksAndAMissingCameraNeverShowTheWindow() {
        QSettings s(dir.path() + "/b.ini", QSettings::IniFormat); Settings prefs(&s); AppState st(&s, nullptr, [] { return oneCamera(); });
        FakeBackend b; Engine e(&b, dir.path()); e.setCameraProvider([&] { auto c = st.cameraSpec(); c.pip = true; return c; });
        PipController pip(&st, &e, &prefs, &s);
        st.choose(1); e.setPreview(true, 1); QVERIFY(!pip.wanted());        // my screen: no webcam
        st.choose(3); e.setPreview(true, 3); QVERIFY(!pip.wanted());        // just my camera: libobs shows it full frame
        st.choose(4); e.setPreview(true, 4); QVERIFY(!pip.wanted());        // the show: its own layout
        QSettings s2(dir.path() + "/c.ini", QSettings::IniFormat); Settings prefs2(&s2); AppState none(&s2, nullptr, [] { return QList<CameraInfo>(); });
        none.choose(2); Engine e2(&b, dir.path()); PipController pip2(&none, &e2, &prefs2, &s2); QVERIFY(!pip2.wanted());   // no camera to show
    }
    void aStandInPictureIsUsedForFakeCamerasSoTestsNeverOpenARealOne() {
        QSettings s(dir.path() + "/d.ini", QSettings::IniFormat); Settings prefs(&s);
        AppState st(&s, nullptr, [] { return QList<CameraInfo>{{"fake:brio", "Logitech BRIO", true}}; });
        FakeBackend b; Engine e(&b, dir.path()); PipController pip(&st, &e, &prefs, &s);
        QVERIFY(pip.testPattern()); QVERIFY(pip.device().isNull());
    }
    void changesAreAnnouncedWhenAnythingItDependsOnChanges() {
        QSettings s(dir.path() + "/e.ini", QSettings::IniFormat); Settings prefs(&s); AppState st(&s, nullptr, [] { return oneCamera(); });
        FakeBackend b; Engine e(&b, dir.path()); PipController pip(&st, &e, &prefs, &s);
        QSignalSpy changed(&pip, &PipController::changed);
        prefs.setPip(false); QVERIFY(changed.count() >= 1);
        changed.clear(); st.choose(1); QVERIFY(changed.count() >= 1);
    }
};
QTEST_MAIN(PipControllerTests)
#include "tst_pipcontroller.moc"
