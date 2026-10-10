#include "appstate.h"
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

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
};
QTEST_MAIN(AppStateTests)
#include "tst_appstate.moc"
