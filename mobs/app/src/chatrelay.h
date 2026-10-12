#pragma once
#include <QElapsedTimer>
#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

class ChatModel;

// The real chat of the broadcast, from the person's OWN browser on another computer (his Mac on the tailnet), where he is logged in to x.com and
// sees what his viewers see. A small script there reads the chat from the page and POSTs it here; this class listens, puts the lines in the chat
// card under the webcam, and says whether that other computer is alive. Read-only: nothing is ever sent back to X or to a viewer.
//
// What arrives is text typed by strangers. It is trimmed and capped, it is shown as plain text, and it never does anything but appear in the card.
// Only the tailnet can reach it: it listens on the tailnet address, never the LAN, and still refuses a peer that is not this computer or in
// 100.64.0.0/10. There is no login: the tailnet is the security (Fred's rule), and the worst a stranger on it can do is add a line to a card.
//
// The wire contract (also in README.md):
//   POST /chat   {"messages":[{"id":"stable-per-line","name":"viewer","text":"words"}]}   -> 204. An empty list is a heartbeat; a single
//                object {"id":..,"name":..,"text":..} works too. An id seen before is ignored, so the script may resend.
//   GET  /health -> 200 "mobs chat relay". Any call counts as "your other computer is alive".
class ChatRelay : public QObject {
    Q_OBJECT
public:
    // One HTTP request, parsed. status 0 means "not complete yet: wait for more bytes"; any other value is what to answer with.
    struct Request {
        int status = 0;
        QString why;                    // a short plain-text reason, the body of an error answer
        QList<QStringList> messages;    // each one: id, name, text (already cleaned)
        bool contact = false;           // the other computer called: it is alive
    };
    static constexpr int kMaxRequest = 16 * 1024;   // headers and body together
    static constexpr int kMaxMessages = 50;         // per call
    static constexpr int kMaxSockets = 8;           // open at once
    static constexpr int kWaitMs = 3000;            // a caller has this long to finish its request
    static constexpr int kAliveMs = 20000;          // the script calls at least this often while it works

    explicit ChatRelay(ChatModel *chat, QObject *parent = nullptr);
    bool start(const QHostAddress &host, quint16 port);   // false: could not listen (the card then stays hidden)
    quint16 port() const { return m_server.serverPort(); }
    bool listening() const { return m_server.isListening(); }

    static QHostAddress tailnetAddress();                  // `tailscale ip -4`, or a null address when there is no tailscale
    static bool peerAllowed(const QHostAddress &peer);     // this computer, or the tailnet (100.64.0.0/10)
    static Request parse(const QByteArray &raw);           // pure: one whole HTTP request in, what to do with it out (unit-tested)
    static QByteArray reply(int status, const QString &why = QString());
private slots:
    void onConnection();
    void refreshStatus();
private:
    void answer(QTcpSocket *socket, const Request &request);
    ChatModel *m_chat;
    QTcpServer m_server;
    QTimer m_status;
    QElapsedTimer m_contact;   // valid once the other computer has called; elapsed() is the time since its last call
    bool m_alive = false;
    int m_open = 0;
};
