#include "chatmodel.h"
#include <QSignalSpy>
#include <QtTest>

class ChatModelTests : public QObject {
    Q_OBJECT
private slots:
    void messagesArriveInOrderNewestLast() {
        ChatModel m; QSignalSpy counted(&m, &ChatModel::countChanged);
        QVERIFY(m.add("1", "ana", "first")); QVERIFY(m.add("2", "bo", "second"));
        QCOMPARE(m.count(), 2); QCOMPARE(counted.count(), 2);
        QCOMPARE(m.data(m.index(0), ChatModel::TextRole).toString(), QString("first"));
        QCOMPARE(m.data(m.index(1), ChatModel::NameRole).toString(), QString("bo"));
    }
    void aRepeatedIdIsIgnoredButNoIdAlwaysCounts() {
        ChatModel m;
        QVERIFY(m.add("a", "ana", "hello")); QVERIFY(!m.add("a", "ana", "hello again")); QCOMPARE(m.count(), 1);
        QVERIFY(m.add("", "ana", "same words")); QVERIFY(m.add("", "ana", "same words")); QCOMPARE(m.count(), 3);
    }
    void nothingToShowIsIgnored() {
        ChatModel m;
        QVERIFY(!m.add("1", "ana", "")); QVERIFY(!m.add("2", "ana", "   \n\t ")); QCOMPARE(m.count(), 0);
    }
    void aNamelessViewerStillGetsAName() {
        ChatModel m; QVERIFY(m.add("1", "  ", "hi"));
        QCOMPARE(m.data(m.index(0), ChatModel::NameRole).toString(), QString("viewer"));
        QCOMPARE(m.data(m.index(0), ChatModel::InitialRole).toString(), QString("V"));
    }
    void theFlowKeepsTheNewestTwoHundred() {
        ChatModel m;
        for (int i = 0; i < 250; ++i) QVERIFY(m.add(QString::number(i), "ana", QString("m%1").arg(i)));
        QCOMPARE(m.count(), ChatModel::kMax);
        QCOMPARE(m.data(m.index(0), ChatModel::TextRole).toString(), QString("m50"));
        QCOMPARE(m.data(m.index(ChatModel::kMax - 1), ChatModel::TextRole).toString(), QString("m249"));
        QVERIFY(m.add("49", "ana", "an id that fell off the top can come back"));   // it is no longer remembered
    }
    void lineBreaksAreFlattenedAndLongLinesCut() {
        ChatModel m;
        m.add("1", "ana", "one\ntwo   three");
        QCOMPARE(m.data(m.index(0), ChatModel::TextRole).toString(), QString("one two three"));
        m.add("2", QString(100, 'n'), QString(1000, 'x'));
        QCOMPARE(m.data(m.index(1), ChatModel::TextRole).toString().size(), ChatModel::kTextMax);
        QVERIFY(m.data(m.index(1), ChatModel::TextRole).toString().endsWith(QChar(0x2026)));
        QCOMPARE(m.data(m.index(1), ChatModel::NameRole).toString().size(), ChatModel::kNameMax);
    }
    void aNameKeepsItsColourAndIgnoresCase() {
        QCOMPARE(ChatModel::hueFor("Fred"), ChatModel::hueFor("fred"));
        QVERIFY(ChatModel::hueFor("fred") != ChatModel::hueFor("crystal"));
        for (const char *n : {"a", "ana", "somebody with a long name", "ÅÄÖ"}) { const double h = ChatModel::hueFor(n); QVERIFY(h >= 0.0 && h < 1.0); }
        ChatModel m; m.add("1", "fred", "x"); m.add("2", "Fred", "y");
        QCOMPARE(m.data(m.index(0), ChatModel::HueRole).toDouble(), m.data(m.index(1), ChatModel::HueRole).toDouble());
    }
    void anEmojiNameKeepsItsWholeInitial() {
        ChatModel m; m.add("1", QString::fromUtf8("\xF0\x9F\x90\xB1 cat"), "meow");   // a cat face: two UTF-16 units
        QCOMPARE(m.data(m.index(0), ChatModel::InitialRole).toString(), QString::fromUtf8("\xF0\x9F\x90\xB1"));
    }
    void statusAndConfigurationAreAnnouncedOnceEach() {
        ChatModel m; QSignalSpy changed(&m, &ChatModel::statusChanged);
        QVERIFY(!m.configured()); QVERIFY(!m.connected()); QVERIFY(m.status().contains("not connected"));
        m.setConfigured(true); QCOMPARE(changed.count(), 1); m.setConfigured(true); QCOMPARE(changed.count(), 1);
        m.setStatus(true, QString()); QCOMPARE(changed.count(), 2); QVERIFY(m.connected());
        m.setStatus(true, QString()); QCOMPARE(changed.count(), 2);
        m.setConfigured(true, true); QVERIFY(m.demo()); QCOMPARE(changed.count(), 3);
    }
    void clearEmptiesTheFlowAndSaysSo() {
        ChatModel m; m.add("1", "ana", "x"); QSignalSpy counted(&m, &ChatModel::countChanged);
        m.clear(); QCOMPARE(m.count(), 0); QCOMPARE(counted.count(), 1);
        m.clear(); QCOMPARE(counted.count(), 1);
    }
};

QTEST_MAIN(ChatModelTests)
#include "tst_chatmodel.moc"
