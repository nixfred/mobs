#include "chatrelay.h"
#include "chatmodel.h"
#include "mobslog.h"
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTcpSocket>
#include <memory>

namespace {
// Text from strangers: no control characters (a newline is a space), no bidi overrides that would reorder what the card shows, and a size cap.
QString clean(const QString &in, int max) {
    QString out;
    for (const QChar c : in) {
        const ushort u = c.unicode();
        if ((u >= 0x202A && u <= 0x202E) || (u >= 0x2066 && u <= 0x2069) || u == 0x200E || u == 0x200F) continue;
        const bool control = c.category() == QChar::Other_Control || c == QChar::LineSeparator || c == QChar::ParagraphSeparator;
        out += control ? QChar(' ') : c;
        if (out.size() >= max) break;
    }
    return out;
}
}

ChatRelay::ChatRelay(ChatModel *chat, QObject *parent) : QObject(parent), m_chat(chat) {
    m_server.setMaxPendingConnections(16);
    connect(&m_server, &QTcpServer::newConnection, this, &ChatRelay::onConnection);
    m_status.setInterval(3000);
    connect(&m_status, &QTimer::timeout, this, &ChatRelay::refreshStatus);
}

bool ChatRelay::start(const QHostAddress &host, quint16 port) {
    if (!m_server.listen(host, port)) {
        MobsLog::write("chat", QString("relay could not listen on %1:%2: %3").arg(host.toString()).arg(port).arg(m_server.errorString()));
        return false;
    }
    MobsLog::write("chat", QString("relay listening on %1:%2: the chat card shows what your other computer sends").arg(host.toString()).arg(m_server.serverPort()));
    m_chat->setConfigured(true, false);   // a real feed: the card is under the webcam from now on, and says what it waits for
    refreshStatus();
    m_status.start();
    return true;
}

QHostAddress ChatRelay::tailnetAddress() {
    QProcess p;
    p.start("tailscale", {"ip", "-4"});
    if (!p.waitForFinished(1500) || p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) { p.kill(); return {}; }
    const QString first = QString::fromUtf8(p.readAllStandardOutput()).split('\n').value(0).trimmed();
    return QHostAddress(first);   // null when it does not parse
}

bool ChatRelay::peerAllowed(const QHostAddress &peer) {
    if (peer.isLoopback()) return true;
    bool ok = false;
    const quint32 v4 = peer.toIPv4Address(&ok);   // an IPv4-mapped IPv6 address converts too
    return ok && (v4 & 0xFFC00000u) == 0x64400000u;   // 100.64.0.0/10, the tailnet
}

ChatRelay::Request ChatRelay::parse(const QByteArray &raw) {
    Request r;
    const int headEnd = raw.indexOf("\r\n\r\n");
    if (headEnd < 0) {
        if (raw.size() > 8192) { r.status = 431; r.why = "headers too large"; }
        return r;   // status 0: not complete yet
    }
    const QList<QByteArray> lines = raw.left(headEnd).split('\n');
    const QList<QByteArray> first = lines.value(0).trimmed().split(' ');
    if (first.size() != 3 || !first[2].startsWith("HTTP/1.")) { r.status = 400; r.why = "not an HTTP request"; return r; }
    const QByteArray method = first[0];
    const QByteArray path = first[1].split('?').value(0);
    int length = -1;
    for (int i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines[i].trimmed();
        const int colon = line.indexOf(':');
        if (colon > 0 && line.left(colon).trimmed().toLower() == "content-length") {
            bool ok = false;
            length = line.mid(colon + 1).trimmed().toInt(&ok);
            if (!ok || length < 0) { r.status = 400; r.why = "bad Content-Length"; return r; }
        }
    }
    if (path == "/health") {
        if (method != "GET") { r.status = 405; r.why = "GET only"; return r; }
        r.status = 200; r.why = "mobs chat relay"; r.contact = true;
        return r;
    }
    if (path != "/chat") { r.status = 404; r.why = "nothing here"; return r; }
    if (method != "POST") { r.status = 405; r.why = "POST only"; return r; }
    if (length < 0) { r.status = 411; r.why = "Content-Length needed"; return r; }
    if (length > kMaxRequest) { r.status = 413; r.why = "too large"; return r; }
    const QByteArray body = raw.mid(headEnd + 4);
    if (body.size() < length) return r;   // the rest of the body is still on its way
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(body.left(length), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) { r.status = 400; r.why = "the body must be a JSON object"; return r; }
    const QJsonObject o = doc.object();
    QJsonArray list;
    if (o.value("messages").isArray()) list = o.value("messages").toArray();
    else if (o.contains("text")) list.append(o);
    if (list.size() > kMaxMessages) { r.status = 413; r.why = "too many messages in one call"; return r; }
    for (const QJsonValue &v : std::as_const(list)) {
        const QJsonObject m = v.toObject();
        const QString text = clean(m.value("text").toString(), 1000);   // anything that is not a string is empty, and skipped
        if (text.simplified().isEmpty()) continue;
        r.messages.append({clean(m.value("id").toString(), 120), clean(m.value("name").toString(), 80), text});
    }
    r.status = 204; r.contact = true;
    return r;
}

QByteArray ChatRelay::reply(int status, const QString &why) {
    static const QHash<int, QByteArray> names{{200, "OK"}, {204, "No Content"}, {400, "Bad Request"}, {404, "Not Found"}, {405, "Method Not Allowed"},
                                              {411, "Length Required"}, {413, "Payload Too Large"}, {431, "Request Header Fields Too Large"}, {503, "Service Unavailable"}};
    const QByteArray body = status == 204 ? QByteArray() : why.toUtf8();
    return "HTTP/1.1 " + QByteArray::number(status) + ' ' + names.value(status, "Error") + "\r\nConnection: close\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: "
           + QByteArray::number(body.size()) + "\r\n\r\n" + body;
}

void ChatRelay::onConnection() {
    while (QTcpSocket *s = m_server.nextPendingConnection()) {
        if (!peerAllowed(s->peerAddress())) {
            MobsLog::write("chat", QString("relay: refused a call from %1 (not this computer, not the tailnet)").arg(s->peerAddress().toString()));
            s->abort(); s->deleteLater();
            continue;
        }
        if (m_open >= kMaxSockets) {
            s->write(reply(503, "busy"));
            s->disconnectFromHost();
            connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
            continue;
        }
        ++m_open;
        auto buffer = std::make_shared<QByteArray>();
        auto done = std::make_shared<bool>(false);
        connect(s, &QObject::destroyed, this, [this] { --m_open; });
        connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
        connect(s, &QTcpSocket::readyRead, this, [this, s, buffer, done] {
            if (*done) { s->readAll(); return; }
            buffer->append(s->readAll());
            Request r = parse(*buffer);
            if (r.status == 0 && buffer->size() > kMaxRequest + 8192) { r.status = 413; r.why = "too large"; }
            if (r.status == 0) return;   // keep waiting for the rest of it
            *done = true;
            answer(s, r);
        });
        QTimer::singleShot(kWaitMs, s, [s] { if (s->state() != QAbstractSocket::UnconnectedState) s->abort(); });   // a caller that never finishes is dropped
    }
}

void ChatRelay::answer(QTcpSocket *s, const Request &r) {
    int added = 0;
    for (const QStringList &m : r.messages) if (m_chat->add(m[0], m[1], m[2])) ++added;
    if (r.contact) {
        const bool firstCall = !m_contact.isValid();
        m_contact.start();
        if (firstCall) MobsLog::write("chat", QString("relay: first call from %1").arg(s->peerAddress().toString()));
        refreshStatus();
    }
    if (added) MobsLog::write("chat", QString("relay: %1 new chat line(s)").arg(added));   // the count only, never the words
    else if (r.status >= 400) MobsLog::write("chat", QString("relay: refused a call (%1: %2)").arg(r.status).arg(r.why));
    s->write(reply(r.status, r.why));
    s->disconnectFromHost();
}

void ChatRelay::refreshStatus() {
    const bool alive = m_contact.isValid() && m_contact.elapsed() < kAliveMs;
    if (alive != m_alive) {
        m_alive = alive;
        MobsLog::write("chat", alive ? "relay: your other computer is connected" : "relay: your other computer went quiet");
    }
    m_chat->setStatus(alive, alive ? QString() : m_contact.isValid() ? QStringLiteral("your other computer's chat script went quiet")
                                                                      : QStringLiteral("waiting for the chat script on your other computer"));
}
