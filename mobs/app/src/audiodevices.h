#pragma once
#include "recorderbackend.h"
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>

// Read-only discovery of PulseAudio/PipeWire sources. No capture stream is opened.
class AudioDevices : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList microphones READ microphones NOTIFY changed)
    Q_PROPERTY(QVariantList desktops READ desktops NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool scanning READ scanning NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit AudioDevices(QObject *parent = nullptr, QString program = "pactl");
    ~AudioDevices() override;
    QVariantList microphones() const { return m_mics; }
    QVariantList desktops() const { return m_desktops; }
    bool ready() const { return m_ready; }
    bool scanning() const { return m_process.state() != QProcess::NotRunning; }
    QString error() const { return m_error; }
    static bool parse(const QByteArray &json, QVariantList *mics, QVariantList *desktops);
    Q_INVOKABLE QString name(const QString &id, bool desktop = false) const;
    QString problem(const AudioSpec &audio, bool mic) const;
    // "System default microphone" is not a device: it follows whatever becomes the default later (a headset plugged in). A microphone the person LOCKS IN has to be a
    // particular one, so locking resolves "default" to the source the system uses right now. Anything else, or a default that is not a listed microphone, comes back as it was.
    Q_INVOKABLE QString resolveDefault(const QString &id) const;
    Q_INVOKABLE void refresh();
signals:
    void changed();
private:
    void fail(const QString &message);
    QProcess m_process;
    QTimer m_timeout;
    QString m_program, m_error;
    QVariantList m_mics, m_desktops;
    bool m_ready = false;
};
