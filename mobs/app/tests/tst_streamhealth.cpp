#include "streamhealth.h"
#include <QtTest>

class StreamHealthTests : public QObject {
    Q_OBJECT
private slots:
    void aSingleQuickDropIsAHiccupNotAVerdict() {
        StreamHealth h; h.connected(1000, 0);
        const auto v = h.dropped(3000, 50000);
        QVERIFY(!v.giveUp); QCOMPARE(h.quickDrops(), 1); QVERIFY(v.note.contains("2.0 s")); QVERIFY(v.note.contains("48 KB"));
    }
    void threeQuickDropsInARowMeanXIsRefusingTheStreamAndSayWhatToDo() {
        StreamHealth h; StreamHealth::Verdict v;
        for (int i = 0; i < 3; ++i) { h.connected(i * 10000, 0); v = h.dropped(i * 10000 + 2000, 100000); if (i < 2) QVERIFY(!v.giveUp); }
        QVERIFY(v.giveUp);
        QVERIFY(v.advice.contains("3 times")); QVERIFY(v.advice.contains("Live Studio")); QVERIFY(v.advice.contains("stream key")); QVERIFY(v.advice.contains("GO LIVE"));
        QVERIFY(!v.advice.endsWith('.'));                                    // the screen adds its own full stop
    }
    void oneLongSessionProvesTheKeyWorksAndStartsTheCountOver() {
        StreamHealth h;
        h.connected(0, 0); h.dropped(2000, 1000); h.connected(5000, 0); h.dropped(7000, 1000); QCOMPARE(h.quickDrops(), 2);
        h.connected(10000, 0); const auto v = h.dropped(10000 + 60000, 5000000); QCOMPARE(h.quickDrops(), 0); QVERIFY(!v.giveUp);   // a minute of video got through
        h.connected(80000, 0); QVERIFY(!h.dropped(82000, 1000).giveUp);     // and the next quick drop is the first again
    }
    void aConnectionThatNeverWentLiveIsNotCounted() {
        StreamHealth h;
        for (int i = 0; i < 5; ++i) QVERIFY(!h.dropped(i * 1000, 0).giveUp);
        QCOMPARE(h.quickDrops(), 0);
    }
    void resetForgetsEverythingForTheNextBroadcast() {
        StreamHealth h; h.connected(0, 0); h.dropped(1000, 0); h.connected(2000, 0); h.dropped(3000, 0); h.reset();
        QCOMPARE(h.quickDrops(), 0); QVERIFY(!h.dropped(10, 0).giveUp);
    }
    void aDropJustUnderAndJustOverTheLimitIsJudgedAccordingly() {
        StreamHealth h; h.connected(0, 0); h.dropped(StreamHealth::kQuickMs - 1, 0); QCOMPARE(h.quickDrops(), 1);
        h.connected(0, 0); h.dropped(StreamHealth::kQuickMs, 0); QCOMPARE(h.quickDrops(), 0);   // exactly the limit is a real session
    }
};
QTEST_MAIN(StreamHealthTests)
#include "tst_streamhealth.moc"
