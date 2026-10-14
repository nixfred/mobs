#pragma once
#include <QList>
#include <algorithm>
#include <QPair>
#include <QtGlobal>
#include <cmath>

// Is the number of people watching going up or down? A reading comes in every few seconds (X itself refreshes the number every 20 seconds), so single readings say little. The trend
// compares what the last few readings say now with what they said about two minutes earlier, and calls it steady until the readings span a minute: a number that has just appeared says
// nothing about where it is going. The same two numbers give the arrow and the "+2 in two minutes" beside it, so they can never disagree.
class ViewerTrend {
public:
    static constexpr qint64 kSpanMinMs = 60000;        // readings must reach back at least this far before there is a trend
    static constexpr qint64 kCompareMs = 120000;       // "two minutes ago"
    static constexpr qint64 kKeepMs = 900000;          // older readings are forgotten
    static constexpr double kStep = 0.75;              // a change of less than this many people (smoothed) is steady

    void clear() { m_samples.clear(); }
    // A reading of how many are watching, at a time in milliseconds (the clock only has to keep going the same way; the last reading is "now").
    void add(int viewers, qint64 atMs) {
        if (viewers < 0) return;
        if (!m_samples.isEmpty() && atMs < m_samples.last().first) return;   // a clock that went backwards says nothing
        m_samples.append({atMs, viewers});
        while (!m_samples.isEmpty() && atMs - m_samples.first().first > kKeepMs) m_samples.removeFirst();
    }
    int current() const { return m_samples.isEmpty() ? -1 : m_samples.last().second; }
    bool ready() const { return m_samples.size() >= 4 && m_samples.last().first - m_samples.first().first >= kSpanMinMs; }
    // How many more (or fewer) are watching now than about two minutes ago, smoothed over the last few readings. 0 until there is a trend.
    int delta() const { return int(std::lround(change())); }
    // +1 up, -1 down, 0 steady or too early to say.
    int trend() const {
        if (!ready()) return 0;
        const double c = change();
        return c >= kStep ? 1 : c <= -kStep ? -1 : 0;
    }
private:
    // The average of the last three readings against the average of three readings from about two minutes before the last one (or from the start, when the readings do not go back that far).
    double change() const {
        if (!ready()) return 0;
        const qint64 now = m_samples.last().first;
        const auto averageAround = [this](qint64 at) {   // the three readings nearest to this time
            QList<QPair<qint64, int>> near = m_samples;
            std::sort(near.begin(), near.end(), [at](const auto &a, const auto &b) { return std::llabs(a.first - at) < std::llabs(b.first - at); });
            double sum = 0; const int n = int(qMin<qsizetype>(3, near.size()));
            for (int i = 0; i < n; ++i) sum += near.at(i).second;
            return n ? sum / n : 0.0;
        };
        const qint64 reach = qMin(kCompareMs, now - m_samples.first().first);
        return averageAround(now) - averageAround(now - reach);
    }
    QList<QPair<qint64, int>> m_samples;
};
