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
    m_folder = m_settings->value("folder").toString();
    m_mic = m_settings->value("mic", true).toBool();
    m_review = m_settings->value("reviewAfter", true).toBool();
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
    if (quality < 0 || quality > 2 || quality == m_quality) return;
    m_quality = quality;
    m_settings->setValue("quality", quality); m_settings->sync();
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

void Settings::openFolder() {
    QDir().mkpath(folder());
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder()));
}
