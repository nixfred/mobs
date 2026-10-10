#include "appstate.h"
#include "engine.h"
#include "obsbackend.h"
#include "palette.h"
#include <QDir>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSettings>
#include <QTimer>
#include <QDebug>
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
    return app.exec();
}
