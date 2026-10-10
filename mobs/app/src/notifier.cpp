#include "notifier.h"
#include "mobslog.h"
#include <QGuiApplication>
#include <QProcess>
#include <QStandardPaths>

void Notifier::send(const QString &title, const QString &body, bool important) {
    MobsLog::write("notify", title + ": " + body);
    if (!QGuiApplication::platformName().contains("wayland") || qEnvironmentVariable("MOBS_NO_NOTIFY") == "1") return;
    const QString bin = QStandardPaths::findExecutable("notify-send");
    if (bin.isEmpty()) return;
    QProcess::startDetached(bin, {"-a", "MOBS", "-i", "mobs", "-t", important ? "15000" : "8000", title, body});
}
