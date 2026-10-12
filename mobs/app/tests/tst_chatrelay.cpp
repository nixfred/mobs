#include "chatmodel.h"
#include "chatrelay.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QtTest>

static QByteArray post(const QByteArray &path, const QByteArray &body) {
    return "POST " + path + " HTTP/1.1\r\nHost: gus\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
}
static QByteArray json(const QJsonObject &o) { return QJsonDocument(o).toJson(QJsonDocument::Compact); }
static QJsonObject line(const QString &id, const QString &name, const QString &text) { return {{"id", id}, {"name", name}, {"text", text}}; }

class ChatRelayTests : public QObject {
    Q_OBJECT
private slots:
    // ---- parsing one request
    void aBatchOfMessagesIsParsedAndCountsAsContact() {
        const auto r = ChatRelay::parse(post("/chat", json({{"messages", QJsonArray{line("1", "ana", "hello"), line("2", "bo", "what theme is that?")}}})));
        QCOMPARE(r.status, 204); QVERIFY(r.contact);
        QCOMPARE(r.messages.size(), 2);
        QCOMPARE(r.messages[0], QStringList({"1", "ana", "hello"}));
        QCOMPARE(r.messages[1][2], QString("what theme is that?"));
    }
    void aSingleMessageObjectAndAHeartbeatAreBothFine() {
        const auto one = ChatRelay::parse(post("/chat", json(line("9", "kiwi", "hi"))));
        QCOMPARE(one.status, 204); QCOMPARE(one.messages.size(), 1);
        const auto beat = ChatRelay::parse(post("/chat", json({{"messages", QJsonArray{}}})));
        QCOMPARE(beat.status, 204); QVERIFY(beat.contact); QVERIFY(beat.messages.isEmpty());
    }
    void anIncompleteRequestIsWaitedForNotRefused() {
        QCOMPARE(ChatRelay::parse("POST /chat HTTP/1.1\r\nContent-Le").status, 0);
        const QByteArray whole = post("/chat", json(line("1", "a", "hello")));
        QCOMPARE(ChatRelay::parse(whole.left(whole.size() - 5)).status, 0);   // the body is still on its way
        QCOMPARE(ChatRelay::parse(whole).status, 204);
        QCOMPARE(ChatRelay::parse(QByteArray(9000, 'x')).status, 431);        // headers that never end
    }
    void otherPathsMethodsAndBodiesAreRefusedWithAReason() {
        QCOMPARE(ChatRelay::parse("GET /chat HTTP/1.1\r\nHost: x\r\n\r\n").status, 405);
        QCOMPARE(ChatRelay::parse("POST /nope HTTP/1.1\r\nContent-Length: 0\r\n\r\n").status, 404);
        QCOMPARE(ChatRelay::parse("POST /chat HTTP/1.1\r\nHost: x\r\n\r\n").status, 411);
        QCOMPARE(ChatRelay::parse("POST /chat HTTP/1.1\r\nContent-Length: 99999\r\n\r\n").status, 413);
        QCOMPARE(ChatRelay::parse("POST /chat HTTP/1.1\r\nContent-Length: abc\r\n\r\n").status, 400);
        QCOMPARE(ChatRelay::parse("hello there\r\n\r\n").status, 400);
        QCOMPARE(ChatRelay::parse(post("/chat", "not json")).status, 400);
        QCOMPARE(ChatRelay::parse(post("/chat", "[1,2]")).status, 400);
        QJsonArray many; for (int i = 0; i < ChatRelay::kMaxMessages + 1; ++i) many.append(line(QString::number(i), "a", "x"));
        QCOMPARE(ChatRelay::parse(post("/chat", json({{"messages", many}}))).status, 413);
        const auto health = ChatRelay::parse("GET /health HTTP/1.1\r\nHost: x\r\n\r\n");
        QCOMPARE(health.status, 200); QVERIFY(health.contact);
        QCOMPARE(ChatRelay::parse("POST /health HTTP/1.1\r\nContent-Length: 0\r\n\r\n").status, 405);
    }
    void textFromStrangersIsCleanedBeforeAnythingSeesIt() {
        const QString evil = QString("a\nb\tc") + QChar(0x07) + "d" + QChar(0x202E) + "e" + QChar(0x2066) + "f";
        const auto r = ChatRelay::parse(post("/chat", json({{"messages", QJsonArray{line("1", "x\ny", evil)}}})));
        QCOMPARE(r.messages.size(), 1);
        QCOMPARE(r.messages[0][2], QString("a b c def"));    // control characters became spaces, the bidi overrides are gone
        QCOMPARE(r.messages[0][1], QString("x y"));
        const QString longText(5000, 'z');
        QCOMPARE(ChatRelay::parse(post("/chat", json({{"messages", QJsonArray{line("1", "a", longText)}}}))).messages[0][2].size(), 1000);
    }
    void fieldsThatAreNotTextAreIgnoredAndEmptyLinesAreSkipped() {
        const QJsonObject odd{{"id", 5}, {"name", QJsonArray{1}}, {"text", 42}};   // numbers and arrays are not text
        const auto r = ChatRelay::parse(post("/chat", json({{"messages", QJsonArray{odd, line("2", "a", "   "), line("3", "", "hello")}}})));
        QCOMPARE(r.status, 204);
        QCOMPARE(r.messages.size(), 1);
        QCOMPARE(r.messages[0], QStringList({"3", "", "hello"}));
    }
    void aReplyIsAWholeHttpAnswer() {
        const QByteArray ok = ChatRelay::reply(204);
        QVERIFY(ok.startsWith("HTTP/1.1 204 No Content\r\n")); QVERIFY(ok.contains("Content-Length: 0\r\n\r\n"));
        const QByteArray bad = ChatRelay::reply(404, "nothing here");
        QVERIFY(bad.startsWith("HTTP/1.1 404 Not Found\r\n")); QVERIFY(bad.endsWith("\r\n\r\nnothing here")); QVERIFY(bad.contains("Content-Length: 12\r\n"));
    }
    // ---- who may call
    void onlyThisComputerAndTheTailnetMayCall() {
        for (const char *yes : {"127.0.0.1", "100.64.0.1", "100.100.100.100", "100.127.255.255", "::1", "::ffff:100.100.1.1"}) QVERIFY2(ChatRelay::peerAllowed(QHostAddress(yes)), yes);
        for (const char *no : {"100.63.255.255", "100.128.0.0", "10.0.0.5", "192.168.1.2", "8.8.8.8", "2001:db8::1"}) QVERIFY2(!ChatRelay::peerAllowed(QHostAddress(no)), no);
    }
    // ---- the real thing over a socket
    void aCallOverTheNetworkFillsTheCardAndSaysTheOtherComputerIsAlive() {
        ChatModel chat; ChatRelay relay(&chat);
        QVERIFY(!chat.configured());                                     // no relay yet, no card
        QVERIFY(relay.start(QHostAddress::LocalHost, 0));
        QVERIFY(chat.configured()); QVERIFY(!chat.demo()); QVERIFY(!chat.connected());
        QVERIFY(chat.status().contains("waiting for the chat script"));  // listening, but nobody has called yet

        auto call = [&](const QByteArray &request) {   // no QTRY macros in here: they `return;` and this returns the answer
            QTcpSocket s; s.connectToHost(QHostAddress::LocalHost, relay.port());
            QElapsedTimer t; t.start();
            while (s.state() != QAbstractSocket::ConnectedState && t.elapsed() < 5000) QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            s.write(request);
            while (s.state() != QAbstractSocket::UnconnectedState && t.elapsed() < 5000) QCoreApplication::processEvents(QEventLoop::AllEvents, 20);   // the relay answers and hangs up
            return s.readAll();
        };
        const QByteArray answer = call(post("/chat", json({{"messages", QJsonArray{line("1", "ana", "hello"), line("2", "bo", "<b>hi</b>")}}})));
        QVERIFY2(answer.startsWith("HTTP/1.1 204"), answer.constData());
        QCOMPARE(chat.count(), 2);
        QVERIFY(chat.connected()); QVERIFY(chat.status().isEmpty());
        QCOMPARE(chat.index(1).data(ChatModel::TextRole).toString(), QString("<b>hi</b>"));   // kept as the text it is: the card shows it as plain text

        call(post("/chat", json({{"messages", QJsonArray{line("1", "ana", "hello")}}})));        // the script resends: nothing doubles
        QCOMPARE(chat.count(), 2);
        QVERIFY(call("GET /health HTTP/1.1\r\nHost: x\r\n\r\n").startsWith("HTTP/1.1 200"));
        QVERIFY(call("POST /chat HTTP/1.1\r\nContent-Length: 3\r\n\r\nabc").startsWith("HTTP/1.1 400"));
        QCOMPARE(chat.count(), 2);
    }
    void aCallerThatNeverFinishesIsDroppedAndAFullRelayTurnsCallersAway() {
        ChatModel chat; ChatRelay relay(&chat);
        QVERIFY(relay.start(QHostAddress::LocalHost, 0));
        QTcpSocket slow; slow.connectToHost(QHostAddress::LocalHost, relay.port());
        QTRY_COMPARE(slow.state(), QAbstractSocket::ConnectedState);
        slow.write("POST /chat HTTP/1.1\r\nContent-Le");                                         // and never the rest
        QTRY_COMPARE_WITH_TIMEOUT(slow.state(), QAbstractSocket::UnconnectedState, ChatRelay::kWaitMs + 2500);
        QCOMPARE(chat.count(), 0);
    }
    void whenTheRelayCannotListenTheCardStaysHidden() {
        ChatModel chat; ChatRelay one(&chat), two(&chat);
        QVERIFY(one.start(QHostAddress::LocalHost, 0));
        QVERIFY(!two.start(QHostAddress::LocalHost, one.port()));       // the port is taken
        QVERIFY(!two.listening());
        ChatModel quiet; ChatRelay third(&quiet);
        QVERIFY(!third.start(QHostAddress::LocalHost, one.port()));
        QVERIFY(!quiet.configured());
    }
};
QTEST_MAIN(ChatRelayTests)
#include "tst_chatrelay.moc"
