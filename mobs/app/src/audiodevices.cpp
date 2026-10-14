#include "audiodevices.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

AudioDevices::AudioDevices(QObject *parent, QString program) : QObject(parent), m_program(std::move(program)) {
    connect(&m_process, &QProcess::stateChanged, this, [this] { emit changed(); });
    m_timeout.setSingleShot(true); m_timeout.setInterval(4000);
    connect(&m_timeout, &QTimer::timeout, this, [this] { m_process.kill(); fail("audio device lookup timed out; refresh to try again"); });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) fail("cannot list audio inputs: pactl is unavailable");
    });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_timeout.stop();
        QVariantList mics, desktops;
        if (code || status != QProcess::NormalExit || !parse(m_process.readAllStandardOutput(), &mics, &desktops)) {
            if (m_error.isEmpty()) fail("cannot reach the audio service; refresh or record with audio off");
            return;
        }
        m_mics = mics; m_desktops = desktops; m_ready = true; m_error.clear(); emit changed();
    });
}
AudioDevices::~AudioDevices() { if (scanning()) { m_process.kill(); m_process.waitForFinished(1000); } }
void AudioDevices::fail(const QString &message) { m_timeout.stop(); m_ready = false; m_mics.clear(); m_desktops.clear(); m_error = message; emit changed(); }

bool AudioDevices::parse(const QByteArray &json, QVariantList *mics, QVariantList *desktops) {
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray()) return false;
    QVariantList inputs, outputs; QSet<QString> seen;
    for (const auto &value : doc.array()) {
        if (!value.isObject()) continue;
        const auto source = value.toObject(); const QString id = source["name"].toString();
        if (id.isEmpty() || seen.contains(id)) continue;
        seen.insert(id);
        QString label = source["description"].toString();
        if (label.isEmpty() || label == "(null)") label = id;
        const auto monitor = source["monitor_source"];
        const auto sink = source["monitor_of_sink"];
        const bool desktop = id.endsWith(".monitor") || (monitor.isString() && !monitor.toString().isEmpty() && monitor.toString() != "n/a")
            || (sink.isDouble() && sink.toDouble() != 4294967295.0) || (sink.isString() && sink.toString() != "n/a" && sink.toString() != "4294967295" && !sink.toString().isEmpty());
        (desktop ? outputs : inputs).append(QVariantMap{{"id", id}, {"name", label}});
    }
    *mics = inputs; *desktops = outputs; return true;
}

QString AudioDevices::name(const QString &id, bool desktop) const {
    if (id == "default" || id.isEmpty()) return desktop ? "system output" : "system microphone";
    for (const auto &item : (desktop ? m_desktops : m_mics)) if (item.toMap()["id"].toString() == id) return item.toMap()["name"].toString();
    return desktop ? "unavailable output" : "unavailable microphone";
}
QString AudioDevices::resolveDefault(const QString &id) const {
    if (id != "default") return id;
    if (qEnvironmentVariable("MOBS_FAKE_AUDIO") == "1") return qEnvironmentVariable("MOBS_FAKE_DEFAULT_SOURCE", id);   // tests: a stand-in system default
    QProcess p;
    p.start(m_program, {"get-default-source"});
    if (!p.waitForStarted(500) || !p.waitForFinished(1000) || p.exitCode() != 0) return id;   // a click must not hang on a wedged audio service: a second and a half at most
    const QString name = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    for (const auto &item : m_mics) if (item.toMap()["id"].toString() == name) return name;
    return id;
}
// Which microphone "the system default" is right now, by the name the list shows ("" when it is not a listed microphone or the audio service does not answer): the
// first row of the microphone list says it, so nobody has to guess that the default is the very microphone they are looking for.
QString AudioDevices::defaultMicName() const {
    const QString id = resolveDefault("default");
    if (id == "default") return {};
    for (const auto &item : m_mics) if (item.toMap()["id"].toString() == id) return item.toMap()["name"].toString();
    return {};
}
QString AudioDevices::problem(const AudioSpec &audio, bool mic) const {
    if (!mic && !audio.desktop) return {};
    if (!m_ready) return m_error.isEmpty() ? "checking audio inputs; try again in a moment" : m_error;
    const auto available = [](const QVariantList &devices, const QString &id) {
        if (id == "default") return !devices.isEmpty();
        for (const auto &item : devices) if (item.toMap()["id"].toString() == id) return true;
        return false;
    };
    if (mic && !available(m_mics, audio.micDevice)) return "the chosen microphone is unavailable: press the microphone button to choose another, or turn it off there";
    if (audio.desktop && !available(m_desktops, audio.desktopDevice)) return "the chosen desktop audio source is unavailable; choose an output in EXPERT > audio or turn desktop audio off";
    return {};
}
void AudioDevices::refresh() {
    if (scanning()) return;
    m_error.clear();
    if (qEnvironmentVariable("MOBS_FAKE_AUDIO") == "1") {
        QByteArray json = R"([{"name":"test:mic","description":"Demo microphone"},{"name":"test:usb","description":"USB microphone"},{"name":"test:third","description":"Headset microphone"},{"name":"test:speakers.monitor","description":"Desktop speakers"},{"name":"test:headphones.monitor","description":"Headphones"}])";
        // MOBS_FAKE_MORE_MICS=N adds N more microphones, for the day a real machine lists five (the Maono was the fifth row on 2026-10-13, below the fold of a list that showed three).
        if (const int more = qBound(0, qEnvironmentVariableIntValue("MOBS_FAKE_MORE_MICS"), 40); more > 0) {
            json.chop(1);
            for (int i = 1; i <= more; ++i) json += QByteArray(R"(,{"name":"test:extra%1","description":"Extra microphone %1"})").replace("%1", QByteArray::number(i));
            json += ']';
        }
        parse(json, &m_mics, &m_desktops);
        m_ready = true; emit changed(); return;
    }
    m_process.start(m_program, {"--format=json", "list", "sources"}); m_timeout.start(); emit changed();
}
