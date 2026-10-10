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
    void recordSaysItIsNotWiredYet() {
        QTemporaryDir d; QSettings s(d.path() + "/m.ini", QSettings::IniFormat);
        AppState a(&s); a.choose(1); a.record();
        QVERIFY(a.note().contains("not wired"));
        a.choose(1);   // choosing again clears the note
        QVERIFY(a.note().isEmpty());
    }
};
QTEST_MAIN(AppStateTests)
#include "tst_appstate.moc"
