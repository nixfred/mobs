#pragma once
#include <QString>
#include <QtGlobal>

// Tells "the network hiccuped" from "X is refusing us". X's ingest gives no error for a key it does not accept: it takes the connection and
// hangs up within seconds, so libobs sees a drop and reconnects, forever (Fred, 2026-10-10: "it's connecting and dropping over and over").
// A connection that dies within kQuickMs of going live, kGiveUpAfter times in a row, is not a hiccup: stop retrying and say what it usually means.
// Pure logic (no libobs), so it is unit-tested.
class StreamHealth {
public:
    static constexpr qint64 kQuickMs = 15000;
    static constexpr int kGiveUpAfter = 3;
    struct Verdict { bool giveUp = false; QString advice; QString note; };

    void reset() { m_quick = 0; m_liveAt = -1; m_bytesAtLive = 0; }
    void connected(qint64 nowMs, quint64 totalBytes) { m_liveAt = nowMs; m_bytesAtLive = totalBytes; }
    int quickDrops() const { return m_quick; }

    // The connection that was live just ended. `note` is for the log; `giveUp` + `advice` when it is time to stop retrying.
    Verdict dropped(qint64 nowMs, quint64 totalBytes) {
        Verdict v;
        if (m_liveAt < 0) return v;                       // it never went live: a failed connect is reported by its own code, not counted here
        const qint64 lasted = nowMs - m_liveAt;
        const quint64 sent = totalBytes >= m_bytesAtLive ? totalBytes - m_bytesAtLive : 0;
        m_liveAt = -1;
        m_quick = lasted < kQuickMs ? m_quick + 1 : 0;    // one long session in between proves the key works: the count starts over
        v.note = QString("that connection lasted %1 s and sent %2 KB (quick drops in a row: %3)").arg(lasted / 1000.0, 0, 'f', 1).arg(sent / 1024).arg(m_quick);
        if (m_quick >= kGiveUpAfter) {
            v.giveUp = true;
            v.advice = QString("X hung up within seconds, %1 times in a row. The usual cause: X no longer accepts this stream key (a key stops working when its "
                               "livestream ends). In X Live Studio (studio.x.com/live) create a New livestream with source RTMP, paste its new key in EXPERT > live, "
                               "then press GO LIVE").arg(m_quick);
        }
        return v;
    }
private:
    int m_quick = 0;
    qint64 m_liveAt = -1;
    quint64 m_bytesAtLive = 0;
};
