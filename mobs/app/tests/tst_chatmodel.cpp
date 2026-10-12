#include "chatmodel.h"
#include <QClipboard>
#include <QGuiApplication>
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
    // ---- the chat of the person's own livestream, shown by X's own page
    void aLiveStudioLinkIsFoundAndNothingElseIsTakenForOne() {
        const QString id = "1PJqrNXnQywxb";
        QCOMPARE(ChatModel::xIdFrom("https://x.com/i/live-studio/" + id), id);
        QCOMPARE(ChatModel::xIdFrom("  https://x.com/i/broadcasts/" + id + "?s=20 \n"), id);
        QCOMPARE(ChatModel::xIdFrom("https://studio.x.com/i/live-studio/" + id), id);
        QCOMPARE(ChatModel::xIdFrom("https://studio.x.com/live/" + id + "?rwebShell=1"), id);   // the address Live Studio really uses
        QCOMPARE(ChatModel::xIdFrom("look: https://twitter.com/i/broadcasts/" + id + "/ ok"), id);
        for (const char *no : {"", "hello", "1PJqrNXnQywxb", "https://x.com/home", "https://x.com/i/live-studio/short", "https://x.com/i/live-studio/",
                               "https://evilx.com/i/live-studio/1PJqrNXnQywxb", "https://x.com.evil.example/i/live-studio/1PJqrNXnQywxb", "https://x.com/i/spaces/1PJqrNXnQywxb", "https://studio.x.com/live/short", "https://evilstudio.x.com/live/1PJqrNXnQywxb", "https://x.com/live/1PJqrNXnQywxb"})
            QVERIFY2(ChatModel::xIdFrom(no).isEmpty(), no);
    }
    void attachingTheXChatMakesTheCardAndDetachingTakesItAway() {
        ChatModel m; QSignalSpy changed(&m, &ChatModel::statusChanged);
        QVERIFY(!m.configured()); QVERIFY(m.xId().isEmpty());
        QVERIFY(!m.attachX("not a link")); QVERIFY(!m.configured());
        QVERIFY(m.attachX("https://x.com/i/live-studio/1PJqrNXnQywxb"));
        QCOMPARE(m.xId(), QString("1PJqrNXnQywxb")); QVERIFY(m.configured()); QVERIFY(m.connected()); QVERIFY(!m.demo());
        const int announced = changed.count();
        QVERIFY(m.attachX("https://x.com/i/broadcasts/1PJqrNXnQywxb")); QCOMPARE(changed.count(), announced);   // the same livestream again: nothing to announce
        m.detachX(); QVERIFY(m.xId().isEmpty()); QVERIFY(!m.configured());                                  // the card existed only because of it
        ChatModel relay; relay.setConfigured(true);                                                          // a chat that is there for another reason keeps its card
        QVERIFY(relay.attachX("https://x.com/i/live-studio/1PJqrNXnQywxb")); relay.detachX(); QVERIFY(relay.configured());
    }
    void aLinkOnTheClipboardIsAttachedOnlyOnTheOffscreenPlatform() {
        if (QGuiApplication::platformName() != "offscreen") QSKIP("this test writes to the clipboard: only on the offscreen platform, never on a real desktop");
        ChatModel m;
        QGuiApplication::clipboard()->setText("nothing useful"); QVERIFY(!m.attachXFromClipboard());
        QVERIFY(m.xClipboardNote().isEmpty());                                                              // not an X link: nothing to say, and nothing of it is shown
        QGuiApplication::clipboard()->setText("https://x.com/NixFred/status/1234567890123456789"); QVERIFY(!m.attachXFromClipboard());
        QVERIFY(!m.xClipboardNote().isEmpty());                                                             // an X link that is no livestream link: say what to copy instead
        QGuiApplication::clipboard()->setText(""); QVERIFY(m.xClipboardNote().isEmpty());
        QGuiApplication::clipboard()->setText("https://x.com/i/live-studio/1PJqrNXnQywxb"); QVERIFY(m.attachXFromClipboard()); QCOMPARE(m.xId(), QString("1PJqrNXnQywxb"));
    }
};

QTEST_MAIN(ChatModelTests)
#include "tst_chatmodel.moc"
