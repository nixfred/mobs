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
    void livestreamsThatWereOnAirBeforeTheBroadcastAreNeverAttached() {
        ChatModel m;
        m.setStaleIds({"1OLDoldOLDold1"});
        QVERIFY(!m.attachX("https://studio.x.com/live/1OLDoldOLDold1")); QVERIFY(m.xId().isEmpty());       // an old one: refused, a pasted link too
        QVERIFY(m.attachX("https://studio.x.com/live/1NEWnewNEWnew1")); QCOMPARE(m.xId(), QString("1NEWnewNEWnew1"));
        QVERIFY(m.attachX("https://x.com/i/broadcasts/1NEXTnextNEXT1")); QCOMPARE(m.xId(), QString("1NEXTnextNEXT1"));   // the right one may replace a wrong one
        m.detachX();
        QVERIFY(m.attachX("https://studio.x.com/live/1OLDoldOLDold1"));                                      // the next broadcast starts clean
    }
    void aLinkTypedByHandMayBeOneThatWasOnAirBeforeButTheFinderNeverIs() {
        ChatModel m;
        m.setStaleIds({"1OLDoldOLDold1", "1ALSOalsoALSO1"});
        QVERIFY(!m.attachX("https://studio.x.com/live/1OLDoldOLDold1"));                                    // what the finder or the clipboard finds: refused
        QVERIFY(m.attachXByHand("https://studio.x.com/live/1OLDoldOLDold1")); QCOMPARE(m.xId(), QString("1OLDoldOLDold1"));   // what the person typed: they said which one it is
        QVERIFY(m.isStale("1ALSOalsoALSO1")); QVERIFY(!m.isStale("1OLDoldOLDold1"));                       // only that one is let off
        QVERIFY(!m.attachXByHand("not a link")); QCOMPARE(m.xId(), QString("1OLDoldOLDold1"));              // and a non-link changes nothing
    }
    void typedTextThatIsNotATakenLinkGetsAnExplanation() {
        ChatModel m;
        QVERIFY(m.xTypedNote("").isEmpty()); QVERIFY(m.xTypedNote("   ").isEmpty());                         // nothing typed: nothing to say
        QVERIFY(m.xTypedNote("https://studio.x.com/live/1PJqrNXnQywxb").isEmpty());                         // a link that will be taken
        QVERIFY(!m.xTypedNote("hello").isEmpty()); QVERIFY(!m.xTypedNote("https://example.com/x").isEmpty());
    }
    void aFinderNoteStaysUntilALivestreamIsAttachedOrTheTakeEnds() {
        ChatModel m; QSignalSpy changed(&m, &ChatModel::statusChanged);
        m.setFinderNote("two are on air"); QCOMPARE(m.finderNote(), QString("two are on air")); QCOMPARE(changed.count(), 1);
        m.setFinderNote("two are on air"); QCOMPARE(changed.count(), 1);                                    // the same note again: nothing to announce
        QVERIFY(m.attachX("https://studio.x.com/live/1PJqrNXnQywxb")); QVERIFY(m.finderNote().isEmpty());   // a livestream is attached: the note has done its job
        m.setFinderNote("late"); m.detachX(); QVERIFY(m.finderNote().isEmpty());                            // and the next broadcast starts without it
    }
    void aLinkThatWasOnTheClipboardBeforeTheBroadcastIsNeverPickedUpByItself() {
        if (QGuiApplication::platformName() != "offscreen") QSKIP("this test writes to the clipboard: only on the offscreen platform, never on a real desktop");
        ChatModel m;
        QGuiApplication::clipboard()->setText("https://studio.x.com/live/1OLDoldOLDold1");
        m.rememberClipboard();                                                                              // the broadcast begins with an old livestream's address on the clipboard
        QVERIFY(!m.attachXFromClipboard()); QVERIFY(m.xId().isEmpty()); QVERIFY(m.xClipboardNote().isEmpty());   // not taken, and not commented on
        QGuiApplication::clipboard()->setText("https://studio.x.com/live/1NEWnewNEWnew1");
        QVERIFY(m.attachXFromClipboard()); QCOMPARE(m.xId(), QString("1NEWnewNEWnew1"));                    // copied after the broadcast began: this one is taken
        m.detachX();
        QGuiApplication::clipboard()->setText("https://studio.x.com/live/1OLDoldOLDold1");
        QVERIFY(m.attachXFromClipboard());                                                                  // the next broadcast starts clean: nothing was remembered
    }
    void anAddressFromAnEarlierBroadcastIsNeverPickedUpFromTheClipboardAndTheOldOneIsComparedWithoutSpaces() {
        if (QGuiApplication::platformName() != "offscreen") QSKIP("this test writes to the clipboard: only on the offscreen platform, never on a real desktop");
        ChatModel m; QSignalSpy attached(&m, &ChatModel::attached);
        m.setUsedIds({"1LASTlastLAST1"});
        QGuiApplication::clipboard()->setText("https://studio.x.com/live/1LASTlastLAST1");
        QVERIFY(!m.attachXFromClipboard()); QVERIFY(!m.xClipboardNote().isEmpty());                        // an earlier broadcast's, wherever it was copied: refused, and why
        QVERIFY(m.attachXByHand("https://studio.x.com/live/1LASTlastLAST1")); QCOMPARE(attached.count(), 1);   // typed by the person: always taken
        QCOMPARE(attached.at(0).at(0).toString(), QString("1LASTlastLAST1"));
        m.detachX();
        QGuiApplication::clipboard()->setText("  https://studio.x.com/live/1NEWnewNEWnew1  \n");
        QVERIFY(m.attachXFromClipboard()); QCOMPARE(m.xId(), QString("1NEWnewNEWnew1"));                    // a new one, even with spaces and a line break around it
        ChatModel n;
        QGuiApplication::clipboard()->setText("https://studio.x.com/live/1OLDoldOLDold1\n"); n.rememberClipboard();
        QGuiApplication::clipboard()->setText("https://studio.x.com/live/1OLDoldOLDold1");                  // a clipboard manager gave it back without the line break
        QVERIFY(!n.attachXFromClipboard());
    }
    void aLinkOnTheClipboardIsAttachedOnlyOnTheOffscreenPlatform() {
        if (QGuiApplication::platformName() != "offscreen") QSKIP("this test writes to the clipboard: only on the offscreen platform, never on a real desktop");
        ChatModel m;
        QGuiApplication::clipboard()->setText("nothing useful"); QVERIFY(!m.attachXFromClipboard());
        QVERIFY(m.xClipboardNote().isEmpty());                                                              // not an X link: nothing to say, and nothing of it is shown
        QGuiApplication::clipboard()->setText("https://x.com/NixFred/status/1234567890123456789"); QVERIFY(!m.attachXFromClipboard());
        QVERIFY(!m.xClipboardNote().isEmpty());                                                             // an X link that is no livestream link: say what to copy instead
        QGuiApplication::clipboard()->setText(""); QVERIFY(m.xClipboardNote().isEmpty());
        m.setStaleIds({"1OLDoldOLDold1"});
        QGuiApplication::clipboard()->setText("https://x.com/i/broadcasts/1OLDoldOLDold1"); QVERIFY(!m.attachXFromClipboard()); QVERIFY(!m.xClipboardNote().isEmpty());   // an old livestream's link: refused, and why
        QGuiApplication::clipboard()->setText("https://x.com/i/live-studio/1PJqrNXnQywxb"); QVERIFY(m.attachXFromClipboard()); QCOMPARE(m.xId(), QString("1PJqrNXnQywxb"));
    }
};

QTEST_MAIN(ChatModelTests)
#include "tst_chatmodel.moc"
