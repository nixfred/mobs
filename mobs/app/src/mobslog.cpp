#include "mobslog.h"
#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <cstdio>

namespace {
QMutex g_mutex;
QFile g_file;
QString g_secret;
QtMessageHandler g_previous = nullptr;

void qtHandler(QtMsgType type, const QMessageLogContext &context, const QString &message) {
    static const char *const names[] = {"debug", "warning", "critical", "fatal", "info"};
    MobsLog::write("qt", QString("%1: %2").arg(names[type < 5 ? type : 0], message));
    if (g_previous) g_previous(type, context, message);
    else std::fprintf(stderr, "%s\n", qPrintable(message));
}
}

void MobsLog::init() {
    QMutexLocker lock(&g_mutex);
    if (g_file.isOpen()) return;
    QString file = qEnvironmentVariable("MOBS_LOG_FILE");
    if (file.isEmpty()) file = qEnvironmentVariable("XDG_STATE_HOME", QDir::homePath() + "/.local/state") + "/mobs/mobs.log";
    QDir().mkpath(QFileInfo(file).absolutePath());
    if (QFileInfo(file).size() > 4 * 1024 * 1024) {              // rotate: log -> log.1 -> log.2
        QFile::remove(file + ".2"); QFile::rename(file + ".1", file + ".2"); QFile::rename(file, file + ".1");
    }
    g_file.setFileName(file);
    if (!g_file.open(QIODevice::Append | QIODevice::Text)) { std::fprintf(stderr, "mobs: cannot open the log file %s\n", qPrintable(file)); return; }
    g_file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);   // it names screens, cameras and servers: nobody else's business
    g_previous = qInstallMessageHandler(qtHandler);
}

QString MobsLog::path() { QMutexLocker lock(&g_mutex); return g_file.isOpen() ? g_file.fileName() : QString(); }

void MobsLog::secret(const QString &text) { QMutexLocker lock(&g_mutex); g_secret = text; }

void MobsLog::write(const char *category, const QString &text) {
    QMutexLocker lock(&g_mutex);
    if (!g_file.isOpen()) return;
    QString line = text;
    if (!g_secret.isEmpty()) line.replace(g_secret, "***");
    line.replace('\n', ' ');
    const QByteArray out = (QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz") + " [" + category + "] " + line + "\n").toUtf8();
    g_file.write(out);
    g_file.flush();
}
