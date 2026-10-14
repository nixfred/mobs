#include "viewertrend.h"
#include <QtTest>

class ViewerTrendTests : public QObject {
    Q_OBJECT
    // readings every 10 seconds, from a list
    static ViewerTrend feed(const QList<int> &counts, qint64 stepMs = 10000) {
        ViewerTrend t; qint64 at = 1000000;
        for (int n : counts) { t.add(n, at); at += stepMs; }
        return t;
    }
private slots:
    void nothingToSayBeforeTheFirstReading() {
        ViewerTrend t; QCOMPARE(t.current(), -1); QCOMPARE(t.trend(), 0); QCOMPARE(t.delta(), 0); QVERIFY(!t.ready());
    }
    void aFreshNumberIsSteadyUntilItReachesBackAMinute() {
        ViewerTrend t = feed({3, 5, 7, 9});               // 30 seconds of readings, climbing fast
        QCOMPARE(t.current(), 9); QVERIFY(!t.ready()); QCOMPARE(t.trend(), 0); QCOMPARE(t.delta(), 0);
    }
    void risingReadingsAreUp() {
        ViewerTrend t = feed({3, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8});   // two minutes, 3 to 8
        QVERIFY(t.ready()); QCOMPARE(t.trend(), 1); QVERIFY(t.delta() >= 3);
    }
    void fallingReadingsAreDown() {
        ViewerTrend t = feed({12, 12, 11, 11, 10, 9, 9, 8, 8, 7, 7, 6, 6});
        QCOMPARE(t.trend(), -1); QVERIFY(t.delta() <= -3);
    }
    void aNumberThatStaysPutIsSteady() {
        ViewerTrend t = feed({5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5});
        QCOMPARE(t.trend(), 0); QCOMPARE(t.delta(), 0);
    }
    void aWobbleAroundTheSameNumberIsSteady() {
        ViewerTrend t = feed({5, 6, 5, 5, 6, 5, 5, 6, 5, 6, 5, 5, 6});    // one person in and out
        QCOMPARE(t.trend(), 0);
    }
    void onlyTheLastTwoMinutesCount() {
        // a long climb that has stopped is steady now
        QList<int> counts; for (int i = 0; i < 30; ++i) counts << 2 + i;     // 5 minutes, climbing
        for (int i = 0; i < 14; ++i) counts << 31;                            // then flat for over two minutes
        ViewerTrend t = feed(counts); QCOMPARE(t.trend(), 0);
    }
    void aShortHistoryIsComparedFromItsStart() {
        ViewerTrend t = feed({2, 2, 2, 3, 3, 4, 4});                          // 60 seconds: just enough
        QVERIFY(t.ready()); QCOMPARE(t.trend(), 1);
    }
    void unknownReadingsAndBackwardsClocksAreIgnored() {
        ViewerTrend t; t.add(4, 1000); t.add(-1, 2000); QCOMPARE(t.current(), 4);
        t.add(9, 500); QCOMPARE(t.current(), 4);                              // earlier than the last one
        t.clear(); QCOMPARE(t.current(), -1);
    }
    void oldReadingsAreForgotten() {
        ViewerTrend t; t.add(50, 0); t.add(2, ViewerTrend::kKeepMs + 10000); QCOMPARE(t.current(), 2); QVERIFY(!t.ready());
    }
};
QTEST_MAIN(ViewerTrendTests)
#include "tst_viewertrend.moc"
