#include "guestcall.h"
#include <QJSEngine>
#include <QSignalSpy>
#include <QtTest>

class GuestCallTests : public QObject {
    Q_OBJECT
private slots:
    void aRoomNameIsLongRandomAndNeverRepeats() {
        const QString a = GuestCall::newRoom(), b = GuestCall::newRoom();
        QVERIFY(GuestCall::validRoom(a)); QVERIFY(GuestCall::validRoom(b)); QVERIFY(a != b);
        QCOMPARE(a.size(), 5 + GuestCall::kRoomChars);
        QVERIFY(!GuestCall::validRoom("mobs-short"));
        QVERIFY(!GuestCall::validRoom("other-" + QString(GuestCall::kRoomChars, 'a')));
        QVERIFY(!GuestCall::validRoom("mobs-" + QString(GuestCall::kRoomChars, 'l')));   // l, i, o, 0 and 1 are left out: nothing to misread
    }
    void theViewersLinkHasNoSettingsAndYourSideNeverUsesYourCamera() {
        const QString room = GuestCall::newRoom();
        QCOMPARE(GuestCall::guestUrl(room).toString(), "https://meet.jit.si/" + room);
        const QString host = QString::fromLatin1(GuestCall::hostUrl(room, "Fred").toEncoded());
        QVERIFY(host.startsWith("https://meet.jit.si/" + room + "#"));
        QVERIFY(host.contains("config.startWithVideoMuted=true"));
        QVERIFY(host.contains("config.prejoinConfig.enabled=false"));
        QVERIFY(host.contains("config.disableDeepLinking=true"));
        QVERIFY(host.contains("userInfo.displayName=%22Fred%22"));
    }
    void aNameCannotSmuggleASettingIntoTheLink() {
        QCOMPARE(GuestCall::cleanName("A\"b\\c/d%e"), QString("Abcde"));
        QCOMPARE(GuestCall::cleanName("  sunny   day  "), QString("sunny day"));
        QCOMPARE(GuestCall::cleanName(QString(100, 'n')).size(), 32);
        const QString host = QString::fromLatin1(GuestCall::hostUrl(GuestCall::newRoom(), "x\"&config.evil=1").toEncoded());
        QCOMPARE(host.count("userInfo.displayName="), 1);
        QVERIFY(!host.contains("&config.evil="));
    }
    void theInviteNamesTheViewerWhenThereIsOneAndNeverCarriesYourSettings() {
        const QUrl link = GuestCall::guestUrl(GuestCall::newRoom());
        const QString named = GuestCall::inviteText(link, "sunny"), plain = GuestCall::inviteText(link, "");
        QVERIFY(named.startsWith("Hi sunny! ")); QVERIFY(!plain.startsWith("Hi"));
        QVERIFY(named.contains(link.toString())); QVERIFY(plain.contains(link.toString()));
        QVERIFY(!named.contains("config.")); QVERIFY(!named.contains("#"));
        QVERIFY(named.contains("you can leave any time"));
    }
    void aReportIsAcceptedOnlyInTheShapeThisVersionKnows() {
        bool joined = false; QStringList others;
        QVERIFY(GuestCall::parseReport(R"({"v":1,"joined":true,"others":[{"name":"Ana"},{"name":""}]})", &joined, &others));
        QVERIFY(joined); QCOMPARE(others, (QStringList{"Ana", "guest"}));
        QVERIFY(!GuestCall::parseReport(R"({"v":2,"joined":true,"others":[]})", &joined, &others));
        QVERIFY(!GuestCall::parseReport(R"({"v":1,"others":[]})", &joined, &others));
        QVERIFY(!GuestCall::parseReport("not json", &joined, &others));
        QVERIFY(GuestCall::parseReport("{\"v\":1,\"joined\":true,\"others\":[{\"name\":\"a\\\"b<script>\"}]}", &joined, &others));
        QCOMPARE(others, QStringList{"abscript"});   // markup and quotes never reach the card
    }
    void inviteOpensARoomAndWritesTheInviteForYou() {
        GuestCall c(true); QSignalSpy changed(&c, &GuestCall::changed);
        QVERIFY(!c.active()); QCOMPARE(c.state(), QString("idle")); QVERIFY(c.url().isEmpty());
        c.invite("sunny");
        QVERIFY(c.active()); QCOMPARE(c.state(), QString("waiting")); QCOMPARE(c.label(), QString("sunny"));
        QVERIFY(c.status().contains("sunny")); QVERIFY(c.status().contains("clipboard"));
        QVERIFY(c.lastInvite().startsWith("Hi sunny!")); QVERIFY(c.lastInvite().contains("https://meet.jit.si/mobs-"));
        QVERIFY(!c.lastInvite().contains("config."));    // the viewer's link has none of YOUR side's settings
        QVERIFY(c.url().toString().contains("https://meet.jit.si/mobs-")); QVERIFY(changed.count() >= 1);
        const QUrl first = c.url();
        c.invite("somebody else");                                   // a second invite never swaps the guest or opens another room
        QCOMPARE(c.url(), first); QCOMPARE(c.guest(), QString("sunny"));
    }
    void anInviteWithNobodyNamedStillWorks() {
        GuestCall c(true); c.invite();
        QVERIFY(c.active()); QCOMPARE(c.label(), QString("guest")); QVERIFY(c.status().contains("your guest"));
        QVERIFY(!c.lastInvite().startsWith("Hi"));
    }
    void aGuestJoinsAndLeavesAndTheCallEnds() {
        GuestCall c(true); c.invite("sunny");
        c.report(R"({"v":1,"joined":true,"others":[{"name":"Sunny Day"}]})");
        QCOMPARE(c.state(), QString("connected")); QCOMPARE(c.label(), QString("Sunny Day"));
        c.report(R"({"v":1,"joined":true,"others":[]})");
        QCOMPARE(c.state(), QString("waiting")); QVERIFY(c.status().contains("left")); QCOMPARE(c.label(), QString("sunny"));
        c.report(R"({"v":2})");                                       // a report from a newer page is ignored, not guessed at
        QCOMPARE(c.state(), QString("waiting"));
        c.end();
        QVERIFY(!c.active()); QCOMPARE(c.label(), QString("guest")); QVERIFY(c.status().isEmpty()); QVERIFY(c.url().isEmpty());
        c.report(R"({"v":1,"joined":true,"others":[{"name":"late"}]})");   // a report after the call ended changes nothing
        QVERIFY(!c.active());
    }
    void anUnnamedCallTakesTheNameOfWhoComesIn() {
        GuestCall c(true); c.invite();
        c.report(R"({"v":1,"joined":true,"others":[{"name":"Marlow"}]})");
        QCOMPARE(c.label(), QString("Marlow")); QCOMPARE(c.guest(), QString("Marlow"));
    }
    void aSignInWindowNeverOpensDuringATake() {
        GuestCall c(true); bool take = false;
        c.setTakeProbe([&] { return take; });
        c.invite();
        take = true;
        QVERIFY(!c.signInAllowed()); QVERIFY(c.status().contains("BEFORE you start a take"));
        take = false; c.takeChanged();
        QVERIFY(!c.status().contains("BEFORE")); QVERIFY(c.signInAllowed());
    }
    void aPageThatDoesNotLoadSaysSo() {
        GuestCall c(false); c.invite();    // not a stand-in, but nothing here loads a page: the clipboard may or may not exist, the status is what matters
        c.pageLoaded(false, "net::ERR_INTERNET_DISCONNECTED");
        QVERIFY(c.status().contains("could not open the call")); QVERIFY(c.status().contains("ERR_INTERNET_DISCONNECTED"));
        c.pageLoaded(true, QString());
        QVERIFY(c.pageReady()); QVERIFY(!c.status().contains("could not open"));
    }
    void thePageScriptReportsWhoIsInTheRoom() {
        QJSEngine js;
        js.evaluate("var window = this; var logged = []; var console = { log: function (s) { logged.push(s); } }; function setInterval(f, ms) {}");
        js.evaluate("window.APP = { conference: { isJoined: function () { return true; }, listMembers: function () { return [{ getDisplayName: function () { return 'Ana'; } }, { getDisplayName: function () { return ''; } }]; } } };");
        const QJSValue r = js.evaluate(GuestCall::pageScript());
        QVERIFY2(!r.isError(), qPrintable(r.toString()));
        const QString line = js.evaluate("logged[0]").toString();
        QVERIFY2(line.startsWith("MOBS-CALL "), qPrintable(line));
        bool joined = false; QStringList others;
        QVERIFY(GuestCall::parseReport(line.mid(10), &joined, &others));
        QVERIFY(joined); QCOMPARE(others, (QStringList{"Ana", "guest"}));
    }
    void thePageScriptSaysNotJoinedWhenJitsiIsNotThereOrYouAreNotInTheRoom() {
        for (const char *app : {"", "window.APP = { conference: { isJoined: function () { return false; }, listMembers: function () { return []; } } };",
                                "window.APP = { conference: {} };", "window.APP = { conference: { isJoined: function () { throw new Error('x'); } } };"}) {
            QJSEngine js;
            js.evaluate("var window = this; var logged = []; var console = { log: function (s) { logged.push(s); } }; function setInterval(f, ms) {}");
            js.evaluate(app);
            QVERIFY(!js.evaluate(GuestCall::pageScript()).isError());
            bool joined = true; QStringList others{"x"};
            QVERIFY2(GuestCall::parseReport(js.evaluate("logged[0]").toString().mid(10), &joined, &others), app);
            QVERIFY(!joined); QVERIFY(others.isEmpty());
        }
    }
    void yourNameIsSafeToPutInALink() {
        const QString n = GuestCall::hostName();
        QVERIFY(!n.isEmpty()); QCOMPARE(n, GuestCall::cleanName(n));
    }
};

QTEST_MAIN(GuestCallTests)
#include "tst_guestcall.moc"
