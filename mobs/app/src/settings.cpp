#include "settings.h"
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QUrl>
#include <obs.h>

Settings::Settings(QSettings *settings, QObject *parent) : QObject(parent), m_settings(settings) {
    m_quality = qBound(0, m_settings->value("quality", 1).toInt(), 2);
    bool crfOk = false;
    const int custom = m_settings->value("customCrf", -1).toInt(&crfOk);
    if (crfOk && custom >= 0 && custom <= 51) m_customCrf = custom;
    const QString preset = m_settings->value("encoderPreset", "veryfast").toString();
    if (QStringList{"ultrafast", "superfast", "veryfast", "faster", "fast", "medium", "slow", "slower", "veryslow"}.contains(preset)) m_preset = preset;
    m_folder = m_settings->value("folder").toString();
    m_mic = m_settings->value("mic", true).toBool();
    m_audio.micDevice = m_settings->value("micDevice", "default").toString();
    if (m_audio.micDevice.isEmpty()) m_audio.micDevice = "default";
    m_audio.desktopDevice = m_settings->value("desktopDevice", "default").toString();
    if (m_audio.desktopDevice.isEmpty()) m_audio.desktopDevice = "default";
    m_audio.desktop = m_settings->value("desktopOn", false).toBool();
    const auto gain = [this](const char *key, int fallback) {
        bool ok = false; const int db = m_settings->value(key, fallback).toInt(&ok);
        return ok && db >= -60 && db <= 12 ? db : fallback;
    };
    m_audio.micGainDb = gain("micGainDb", 0);
    m_audio.desktopGainDb = gain("desktopGainDb", -6);
    m_hardware = m_settings->value("hardware", false).toBool();
    m_review = m_settings->value("reviewAfter", true).toBool();
    m_splash = m_settings->value("splash", true).toBool();
}

QString Settings::defaultFolder() { return QStandardPaths::writableLocation(QStandardPaths::MoviesLocation) + "/mobs"; }
QString Settings::folder() const { return m_folder.isEmpty() ? defaultFolder() : m_folder; }

int Settings::crfFor(int quality) {
    static const int crf[] = {28, 23, 18};
    return crf[qBound(0, quality, 2)];
}

QString Settings::aboutText() const {
    return QString("MOBS 0.1 (alpha)\nrecording engine: libobs %1\ninterface: Qt %2\nthe timeline editor is Monologue's (MIT)\nGPLv2, like OBS Studio")
        .arg(QString::fromUtf8(obs_get_version_string()), QString::fromUtf8(qVersion()));
}

QString Settings::problemWith(const QString &folder) {
    if (folder.trimmed().isEmpty()) return "the folder is empty";
    if (!QDir::isAbsolutePath(folder)) return "use a full path, starting with /";
    if (!QDir().mkpath(folder)) return "could not create that folder";
    QTemporaryFile probe(QDir(folder).filePath(".mobs-write-test-XXXXXX"));
    if (!probe.open()) return "MOBS cannot write to that folder";
    return {};
}

void Settings::setQuality(int quality) {
    if (quality < 0 || quality > 2 || (quality == m_quality && !customQuality())) return;
    m_quality = quality;
    m_customCrf = -1;
    m_settings->remove("customCrf");
    m_settings->setValue("quality", quality); m_settings->sync();
    emit changed();
}

void Settings::setCrf(int crf) {
    if (crf < 0 || crf > 51 || crf == m_customCrf) return;
    m_customCrf = crf;
    m_settings->setValue("customCrf", crf); m_settings->sync();
    emit changed();
}

void Settings::setEncoderPreset(const QString &preset) {
    if (preset == m_preset || !QStringList{"ultrafast", "superfast", "veryfast", "faster", "fast", "medium", "slow", "slower", "veryslow"}.contains(preset)) return;
    m_preset = preset;
    m_settings->setValue("encoderPreset", preset); m_settings->sync();
    emit changed();
}

void Settings::resetEncoding() {
    m_customCrf = -1;
    m_preset = "veryfast";
    m_settings->remove("customCrf"); m_settings->remove("encoderPreset"); m_settings->sync();
    emit changed();
}

void Settings::setFolder(const QString &folder) {
    const QString clean = folder.trimmed();
    if (clean == m_folder) return;
    if (!clean.isEmpty() && !problemWith(clean).isEmpty()) return;   // never save a folder that cannot be used
    m_folder = clean;
    if (clean.isEmpty()) m_settings->remove("folder"); else m_settings->setValue("folder", clean);
    m_settings->sync();
    emit changed();
}

void Settings::setMicOn(bool on) {
    if (on == m_mic) return;
    m_mic = on;
    m_settings->setValue("mic", on); m_settings->sync();
    emit changed();
}

void Settings::setReviewAfter(bool on) {
    if (on == m_review) return;
    m_review = on;
    m_settings->setValue("reviewAfter", on); m_settings->sync();
    emit changed();
}

void Settings::setSplash(bool on) {
    if (on == m_splash) return;
    m_splash = on;
    m_settings->setValue("splash", on); m_settings->sync();
    emit changed();
}

void Settings::openFolder() {
    QDir().mkpath(folder());
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder()));
}

void Settings::setMicDevice(const QString &id) {
    if (id.isEmpty() || id == m_audio.micDevice) return;
    m_audio.micDevice = id; m_settings->setValue("micDevice", id); m_settings->sync(); emit changed();
}
void Settings::setDesktopDevice(const QString &id) {
    if (id.isEmpty() || id == m_audio.desktopDevice) return;
    m_audio.desktopDevice = id; m_settings->setValue("desktopDevice", id); m_settings->sync(); emit changed();
}
void Settings::setMicGainDb(int db) {
    if (db < -60 || db > 12 || db == m_audio.micGainDb) return;
    m_audio.micGainDb = db; m_settings->setValue("micGainDb", db); m_settings->sync(); emit changed();
}
void Settings::setDesktopGainDb(int db) {
    if (db < -60 || db > 12 || db == m_audio.desktopGainDb) return;
    m_audio.desktopGainDb = db; m_settings->setValue("desktopGainDb", db); m_settings->sync(); emit changed();
}
void Settings::setDesktopOn(bool on) {
    if (on == m_audio.desktop) return;
    m_audio.desktop = on; m_settings->setValue("desktopOn", on); m_settings->sync(); emit changed();
}
void Settings::setHardware(bool on) {
    if (on == m_hardware) return;
    m_hardware = on; m_settings->setValue("hardware", on); m_settings->sync(); emit changed();
}
