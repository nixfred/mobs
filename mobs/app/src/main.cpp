#include "appstate.h"
#include "engine.h"
#include "obsbackend.h"
#include "palette.h"
#include <QDateTime>
#include <QDir>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSettings>
#include <QTimer>
#include <QDebug>
#include <functional>
#include <memory>
#include <QtQml/qqml.h>

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("mobs");
    app.setOrganizationName("mobs");
    app.setDesktopFileName("mobs");

    // MOBS_THEME_DIR points at a fake Omarchy "current" folder, for tests and screenshots only.
    Palette palette(qEnvironmentVariable("MOBS_THEME_DIR"));
    QSettings settings(qEnvironmentVariable("MOBS_SETTINGS", QSettings().fileName()), QSettings::IniFormat);
    AppState state(&settings);
    if (const int s = qEnvironmentVariableIntValue("MOBS_PICK"); s >= 1 && s <= 3 && state.screen() == 1) state.choose(s);
    if (qEnvironmentVariableIntValue("MOBS_SCREEN") == 1) state.back();

    // Declared before the QML engine so libobs is shut down AFTER the UI is gone (see ObsBackend::~ObsBackend).
    const QString cfg = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/mobs";
    QDir().mkpath(cfg);
    ObsBackend backend(qEnvironmentVariable("MOBS_TOKEN_FILE", cfg + "/screen-token"));
    Engine recorder(&backend, qEnvironmentVariable("MOBS_REC_DIR", QStandardPaths::writableLocation(QStandardPaths::MoviesLocation) + "/mobs"));
    QObject::connect(&recorder, &Engine::stateChanged, &state, [&] { state.setRecording(recorder.active()); });

    qmlRegisterSingletonInstance("Mobs", 1, 0, "Omarchy", &palette);
    qmlRegisterSingletonInstance("Mobs", 1, 0, "Engine", &recorder);
    qmlRegisterSingletonInstance("Mobs", 1, 0, "App", &state);

    QQmlApplicationEngine engine;
    engine.loadFromModule("Mobs", "Main");
    if (engine.rootObjects().isEmpty()) return 1;

    // MOBS_GRAB=/path.png saves the window's own pixels (no desktop around it) and quits: for design checks.
    if (const QString grab = qEnvironmentVariable("MOBS_GRAB"); !grab.isEmpty()) {
        auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QTimer::singleShot(qEnvironmentVariableIntValue("MOBS_GRAB_MS") ?: 2000, &app, [&app, w, grab] {
            qInfo() << "grab" << w->grabWindow().save(grab);
            app.quit();
        });
    }
    // MOBS_SCRIPT drives the app itself, with no synthetic key presses (safe on a live desktop). Steps, comma separated:
    //   start | waitrec | wait=SECONDS | pause | resume | stop | waitidle | grab=/path.png | quit
    // e.g. MOBS_SCRIPT=start,waitrec,wait=3,pause,wait=2,resume,wait=2,stop,waitidle,quit   (for tests and checks only)
    if (const QString script = qEnvironmentVariable("MOBS_SCRIPT"); !script.isEmpty()) {
        auto steps = std::make_shared<QStringList>(script.split(','));
        auto idx = std::make_shared<int>(0);
        auto run = std::make_shared<std::function<void()>>();
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        auto waitFor = [&](std::function<bool()> done, int timeoutMs, const char *what) {
            auto t0 = std::make_shared<qint64>(QDateTime::currentMSecsSinceEpoch());
            auto poll = std::make_shared<std::function<void()>>();
            *poll = [=, &app]() {
                if (done()) { QTimer::singleShot(0, &app, *run); return; }
                if (QDateTime::currentMSecsSinceEpoch() - *t0 > timeoutMs) { qWarning() << "script: timed out waiting for" << what; app.exit(2); return; }
                QTimer::singleShot(200, &app, *poll);
            };
            (*poll)();
        };
        *run = [=, &app, &recorder, &state]() mutable {
            if (*idx >= steps->size()) return;
            const QString st = steps->at((*idx)++);
            std::printf("script: %s\n", qPrintable(st)); std::fflush(stdout);
            if (st == "start") { recorder.start(state.pick()); QTimer::singleShot(0, &app, *run); }
            else if (st == "waitrec") waitFor([&] { return recorder.state() == "recording"; }, 90000, "recording to begin");
            else if (st == "waitidle") waitFor([&] { return recorder.state() == "idle"; }, 30000, "the file to be saved");
            else if (st.startsWith("wait=")) QTimer::singleShot(int(st.mid(5).toDouble() * 1000), &app, *run);
            else if (st == "pause" || st == "resume") { recorder.togglePause(); QTimer::singleShot(0, &app, *run); }
            else if (st == "stop") { recorder.stop(); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("grab=")) { std::printf("script: grab %d\n", int(window->grabWindow().save(st.mid(5)))); QTimer::singleShot(0, &app, *run); }
            else if (st == "quit") app.quit();
            else { qWarning() << "script: unknown step" << st; app.exit(3); }
        };
        QTimer::singleShot(1500, &app, *run);   // let the window appear first
    }
    return app.exec();
}
