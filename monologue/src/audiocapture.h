#pragma once
#include <QObject>
#include <QAudioFormat>
#include <QTimer>
#include <functional>
#include <pulse/pulseaudio.h>

// PulseAudio's capture latency includes unread/server/device buffering. Qt's
// QAudioSource only exposes samples and delivery time, losing that information.
// PipeWire's PulseAudio server implements the same timestamped capture API.
class AudioCapture : public QObject {
    Q_OBJECT
public:
    explicit AudioCapture(std::function<qint64()> clock, QObject *parent = nullptr);
    ~AudioCapture() override;
    void start(const QByteArray &device, const QAudioFormat &format);
    void stop();
    void readAvailable();
    static qint64 captureTime(qint64 deliveredAt, qint64 latency, bool negative) {
        return deliveredAt + (negative ? latency : -latency);
    }
signals:
    void samples(const QByteArray &data, qint64 capturedAt);
    void failed(const QString &message);
private:
    void contextReady();
    void requestTiming();
    void fail(const QString &message);
    void pump();
    std::function<qint64()> m_clock;
    QTimer m_pump;
    QAudioFormat m_format;
    QByteArray m_device;
    pa_mainloop *m_loop = nullptr;
    pa_context *m_context = nullptr;
    pa_stream *m_stream = nullptr;
    bool m_failed = false, m_reading = false, m_timingRequested = false, m_reportedTiming = false;
};
