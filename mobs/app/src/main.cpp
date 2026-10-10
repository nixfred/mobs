#include "appstate.h"
#include "audiodevices.h"
#include "control.h"
#include "engine.h"
#include "obsbackend.h"
#include "palette.h"
#include "previewmodel.h"
#include "review.h"
#include "settings.h"
#include "thumbprovider.h"
#include <QDateTime>
#include <QQmlContext>
#include <QDir>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <QKeyEvent>
#include <QSettings>
#include <QTimer>
#include <QDebug>
#include <functional>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <QtQml/qqml.h>

// Development-script fixture for rendering active UI states offscreen. It never creates media.
// Both MOBS_SCRIPT and MOBS_FAKE_BACKEND must be explicit; ordinary launches always use libobs.
class ScriptBackend final : public RecorderBackend {
public:
    void begin(const RecordSpec &spec) override {
        m_streams = spec.stream.on;
        emit started(); emit audioLevels(-18, -24, spec.mic, spec.audio.desktop, QString());
        if (m_streams) {   // a pretend broadcast: connecting first, live a moment later (the steps below then break it on purpose)
            emit streamState("connecting", QString());
            QTimer::singleShot(400, this, [this] { if (m_streams) { emit streamState("live", QString()); emit streamStats(5800, 0); } });
        }
    }
    void pause(bool) override {}
    void end() override {
        if (qEnvironmentVariable("MOBS_FAKE_STOP_HANG") == "1") return;
        if (m_streams) { m_streams = false; emit streamState("ended", QString()); }
        emit audioLevels(-60, -60, false, false, QString()); emit finished(QString());
    }
    bool m_streams = false;
    void setMicCheck(bool on, const AudioSpec &) override { emit audioLevels(on ? -18 : -60, -60, on, false, QString()); }
    void setPreview(bool on, const RecordSpec &) override {
        if (on) { QImage frame(640, 360, QImage::Format_RGB32); frame.fill(QColor("#3050f0")); emit previewFrame(frame); }
    }
};

int main(int argc, char *argv[]) {
    qputenv("QT_MEDIA_BACKEND", "ffmpeg");   // Qt's ffmpeg multimedia backend plays the takes in the review screen
    QGuiApplication app(argc, argv);
    app.setApplicationName("mobs");
    app.setOrganizationName("mobs");
    app.setDesktopFileName("mobs");

    // MOBS_THEME_DIR points at a fake Omarchy "current" folder, for tests and screenshots only.
    Palette palette(qEnvironmentVariable("MOBS_THEME_DIR"));
    QSettings settings(qEnvironmentVariable("MOBS_SETTINGS", QSettings().fileName()), QSettings::IniFormat);
    const QString cfg = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/mobs";
    QDir().mkpath(cfg);
    // The stream key lives in its own owner-only file, never in the settings file. Scripted runs use a throwaway one (MOBS_STREAM_KEY_FILE).
    Settings prefs(&settings, nullptr, qEnvironmentVariable("MOBS_STREAM_KEY_FILE", cfg + "/stream-key"));
    AudioDevices audio; audio.refresh();
    // MOBS_FAKE_CAMERAS=1: two sample cameras, for screenshots and tests on a machine without a webcam.
    const QString fakeCameras = qEnvironmentVariable("MOBS_FAKE_CAMERAS");
    AppState state(&settings, nullptr, (fakeCameras == "1" || fakeCameras == "many")
        ? AppState::CameraScan([] {
            QList<CameraInfo> cameras{{"fake:brio", "Logitech BRIO", true}, {"fake:hd", "HD Webcam", false}};
            if (qEnvironmentVariable("MOBS_FAKE_CAMERAS") == "many")
                for (int n = 3; n <= 6; ++n) cameras.append({QString("fake:%1").arg(n), QString("Demo camera %1").arg(n), false});
            return cameras;
        })
        : AppState::CameraScan([] { return CameraScanner::scan(); }));
    if (const int s = qEnvironmentVariableIntValue("MOBS_PICK"); s >= 1 && s <= 3 && state.screen() == 1) state.choose(s);
    if (qEnvironmentVariableIntValue("MOBS_SCREEN") == 1) state.back();

    // Declared before the QML engine so libobs is shut down AFTER the UI is gone (see ObsBackend::~ObsBackend).
    std::unique_ptr<RecorderBackend> backend;
    if (qEnvironmentVariable("MOBS_FAKE_BACKEND") == "1" && !qEnvironmentVariable("MOBS_SCRIPT").isEmpty())
        backend = std::make_unique<ScriptBackend>();
    else backend = std::make_unique<ObsBackend>(qEnvironmentVariable("MOBS_TOKEN_FILE", cfg + "/screen-token"));
    Engine recorder(backend.get(), qEnvironmentVariable("MOBS_REC_DIR", QStandardPaths::writableLocation(QStandardPaths::MoviesLocation) + "/mobs"));
    QObject::connect(&recorder, &Engine::forceQuitRequested, &app, [] {
        // A hung encoder may also hang its destructor. This explicit user choice bypasses teardown.
        std::fflush(nullptr);
        std::_Exit(0);
    });
    recorder.setFolderProvider([&] { return prefs.folder(); });
    recorder.setOptionsProvider([&] { return std::make_pair(prefs.crf(), prefs.micOn()); });
    recorder.setAudioProvider([&] { return prefs.audioSpec(); });
    recorder.setHardwareProvider([&] { return prefs.hardware(); });
    recorder.setStreamProvider([&] { return prefs.streamSpec(); });
    recorder.setMicGuard([&] { auto spec = prefs.audioSpec(); spec.desktop = false; return !prefs.micOn() ? QString("turn on the microphone first") : audio.problem(spec, true); });
    recorder.setPresetProvider([&] { return prefs.encoderPreset(); });
    recorder.setCameraProvider([&] { return state.cameraSpec(); });
    QObject::connect(&recorder, &Engine::cameraNeeded, &state, [&] { state.refreshCameras(); state.setCameraPanel(true); });
    QObject::connect(&recorder, &Engine::liveSetupNeeded, &state, [&] { state.openLive(); });
    QObject::connect(&recorder, &Engine::stateChanged, &state, [&] { state.setRecording(recorder.active()); });

    Control control(&recorder, &state);
    if (qEnvironmentVariable("MOBS_NO_HOTKEY") != "1") control.registerOnSessionBus();   // tests that run several copies set MOBS_NO_HOTKEY=1

    Review review;
    const QString openingPrompt = QStringLiteral("wait for the recording to open before recording again");
    recorder.setStartGuard([&] {
        if (review.loading()) return openingPrompt;
        if (review.saving()) return QString("wait for the edit to finish saving before recording again");
        if (review.unsaved()) return QString("save your edit or press done before starting another recording");
        return audio.problem(prefs.audioSpec(), prefs.micOn());
    });
    auto clearReviewGuard = [&] { if (review.isOpen()) recorder.clearIdleMessage(); };
    QObject::connect(&review, &Review::editChanged, &recorder, clearReviewGuard);
    QObject::connect(&review, &Review::saveChanged, &recorder, clearReviewGuard);
    QObject::connect(&review, &Review::openChanged, &state, [&] { state.setReviewing(review.isOpen()); });
    QObject::connect(&review, &Review::openFailed, &recorder, &Engine::reportReviewFailure);
    QObject::connect(&review, &Review::loadingChanged, &recorder, [&, openingPrompt] {
        if (!review.loading() && recorder.message() == openingPrompt) recorder.clearIdleMessage();
    });
    // After a recording is saved, open it for review. Starting a new recording (e.g. by hotkey) closes the review: the take stays on disk.
    QObject::connect(&recorder, &Engine::saved, &review, [&](const QString &path) { if (prefs.reviewAfter()) review.open(path); });
    QObject::connect(&recorder, &Engine::stateChanged, &review, [&] { if (recorder.active() && review.isOpen() && !review.saving()) review.close(); });
    PreviewModel preview;
    QObject::connect(backend.get(), &RecorderBackend::previewFrame, &preview, &PreviewModel::setFrame);
    // The last picture stays while a take is being saved (the recording screen dims it under "saving..."); it is dropped once idle.
    auto dropPictureIfOff = [&] { if (!recorder.previewing() && recorder.state() != "stopping") preview.clear(); };
    QObject::connect(&recorder, &Engine::previewingChanged, &preview, dropPictureIfOff);
    QObject::connect(&recorder, &Engine::stateChanged, &preview, dropPictureIfOff);
    // The preview runs when the user turned it on AND a screen other than "choose" is showing; it follows the pick, the
    // camera and the placement. A recording that starts meanwhile reuses the same scene.
    auto syncPreview = [&] { recorder.setPreview(state.preview() && state.screen() != 1, state.pick()); };
    QObject::connect(backend.get(), &RecorderBackend::previewFailed, &state, [&] { state.setPreview(false); });
    for (auto sig : {&AppState::screenChanged, &AppState::previewChanged, &AppState::pickChanged, &AppState::cameraChanged})
        QObject::connect(&state, sig, &state, syncPreview);

    QObject::connect(&prefs, &Settings::changed, &recorder, [&, previousAudio = prefs.audioSpec(), previousMic = prefs.micOn()]() mutable {
        const auto current = prefs.audioSpec();
        const bool micChanged = previousMic != prefs.micOn() || previousAudio.micDevice != current.micDevice || previousAudio.micGainDb != current.micGainDb;
        const bool inputChanged = previousAudio.micDevice != current.micDevice;
        previousAudio = current; previousMic = prefs.micOn();
        if (!micChanged) return;
        if (recorder.micChecking()) recorder.checkMic(prefs.micOn());
        else if (!prefs.micOn()) recorder.checkMic(false);
        else if (inputChanged) {
            auto microphone = current; microphone.desktop = false;
            if (audio.problem(microphone, true).isEmpty()) recorder.checkMic(false);
        }
    });
    QObject::connect(&state, &AppState::proChanged, &audio, [&] { if (state.pro()) audio.refresh(); });
    QObject::connect(&state, &AppState::screenChanged, &recorder, [&] {
        if ((state.screen() == 1 || state.screen() == 4) && recorder.micChecking()) recorder.checkMic(false);
    });
    qmlRegisterSingletonInstance("MobsCore", 1, 0, "Audio", &audio);
    qmlRegisterSingletonInstance("MobsCore", 1, 0, "Omarchy", &palette);
    qmlRegisterSingletonInstance("MobsCore", 1, 0, "Preview", &preview);
    qmlRegisterSingletonInstance("MobsCore", 1, 0, "Review", &review);
    qmlRegisterSingletonInstance("MobsCore", 1, 0, "Prefs", &prefs);
    qmlRegisterSingletonInstance("MobsCore", 1, 0, "Engine", &recorder);
    qmlRegisterSingletonInstance("MobsCore", 1, 0, "App", &state);

    QStringList qmlWarnings;
    QQmlApplicationEngine engine;
    if (!qEnvironmentVariable("MOBS_SCRIPT").isEmpty()) {
        QObject::connect(&engine, &QQmlEngine::warnings, &app, [&](const QList<QQmlError> &warnings) {
            for (const auto &warning : warnings) { qmlWarnings.append(warning.toString()); std::fprintf(stderr, "script QML: %s\n", qPrintable(warning.toString())); }
        });
    }
    engine.addImageProvider("preview", preview.provider());
    engine.addImageProvider("thumbs", new ThumbProvider(review.thumbs()->store()));   // the timeline filmstrip
    engine.rootContext()->setContextProperty("backend", &review);                       // the name Monologue's EditBar.qml looks up
    // The opening screen plays on an ordinary launch. Scripted runs and screenshot grabs skip it so they land on the real
    // screens (MOBS_SPLASH=1 brings it back for its own tests); MOBS_SPLASH_AT=MS freezes it at that moment, for design checks.
    const QString splashSwitch = qEnvironmentVariable("MOBS_SPLASH");
    const bool splashAtSet = qEnvironmentVariableIsSet("MOBS_SPLASH_AT");
    engine.rootContext()->setContextProperty("splashAllowed", splashSwitch == "1" || splashAtSet ||
        (splashSwitch != "0" && qEnvironmentVariable("MOBS_SCRIPT").isEmpty() && qEnvironmentVariable("MOBS_GRAB").isEmpty()));
    engine.rootContext()->setContextProperty("splashAt", splashAtSet ? qEnvironmentVariableIntValue("MOBS_SPLASH_AT") : -1);
    engine.loadFromModule("Mobs", "Main");
    syncPreview();   // a preview left on last time starts with the app
    if (engine.rootObjects().isEmpty()) return 1;

    auto *testWindow = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (const QString size = qEnvironmentVariable("MOBS_WINDOW_SIZE"); !size.isEmpty()) {
        const auto dimensions = size.split('x');
        if (dimensions.size() == 2) testWindow->resize(dimensions[0].toInt(), dimensions[1].toInt());
    }

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
    //   pick=N | corner=N | size=N | panel=0/1 | preview=0/1   (what the user would click, for screenshots)
    //   grabpreview=/path.png   (save the newest live-preview frame; for checks)
    //   pro=0/1 | tab=N (drawer)   quality=N | mic=0/1
    //   arm=0/1 | streamurl=URL | streamkey=KEY | streamquality=N   (going live; with the fake backend: streamdrop | streamretry | streamback)
    //   review=/path.mp4 | split=SECONDS | cut=INDEX | savereview | waitsaved | closereview   (the editor, for checks)
    // e.g. MOBS_SCRIPT=start,waitrec,wait=3,pause,wait=2,resume,wait=2,stop,waitidle,quit   (for tests and checks only)
    if (const QString script = qEnvironmentVariable("MOBS_SCRIPT"); !script.isEmpty()) {
        auto steps = std::make_shared<QStringList>(script.split(','));
        auto idx = std::make_shared<int>(0);
        auto run = std::make_shared<std::function<void()>>();
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        auto mousePoint = std::make_shared<QPointF>();
        auto waitFor = [run, &app](std::function<bool()> done, int timeoutMs, const char *what) {
            auto t0 = std::make_shared<qint64>(QDateTime::currentMSecsSinceEpoch());
            auto poll = std::make_shared<std::function<void()>>();
            std::weak_ptr<std::function<void()>> weakPoll = poll;
            *poll = [run, t0, done, timeoutMs, what, weakPoll, &app]() {
                if (done()) { QTimer::singleShot(0, &app, *run); return; }
                if (QDateTime::currentMSecsSinceEpoch() - *t0 > timeoutMs) { qWarning() << "script: timed out waiting for" << what; app.exit(2); return; }
                if (auto next = weakPoll.lock()) QTimer::singleShot(200, &app, [next] { (*next)(); });
            };
            (*poll)();
        };
        *run = [=, &app, &recorder, &state, &preview, &review, &prefs, &qmlWarnings, &backend]() mutable {
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
            else if (st.startsWith("pick=")) { state.choose(st.mid(5).toInt()); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("corner=")) { state.setCorner(st.mid(7).toInt()); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("size=")) { state.setSize(st.mid(5).toInt()); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("grabpreview=")) { std::printf("script: grabpreview %d\n", int(preview.image().save(st.mid(12)))); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("pro=")) { state.setPro(st.mid(4) == "1"); QTimer::singleShot(0, &app, *run); }
            else if (st == "openaudio") { state.openAudio(); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("audiodetails=")) {
                if (auto *page = window->findChild<QObject *>("audioPage")) page->setProperty("details", st.mid(13) == "1");
                QTimer::singleShot(0, &app, *run);
            }
            else if (st.startsWith("tab=")) {
                if (auto *panel = window->findChild<QObject *>("expertPanel")) panel->setProperty("tab", st.mid(4).toInt());
                QTimer::singleShot(0, &app, *run);
            }
            else if (st.startsWith("key=")) {
                const QString name = st.mid(4);
                const int key = name == "Space" ? Qt::Key_Space : name == "Esc" ? Qt::Key_Escape : name == "P" ? Qt::Key_P : Qt::Key_unknown;
                QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
                QCoreApplication::sendEvent(window, &press);
                QTimer::singleShot(0, &app, *run);
            }
            else if (st.startsWith("restoregap=")) {
                if (!QMetaObject::invokeMethod(window, "restoreGap", Q_ARG(QVariant, QVariant(st.mid(11).toInt())))) { app.exit(6); return; }
                QTimer::singleShot(0, &app, *run);
            }
            else if (st == "timelinecancel") {
                if (auto *area = window->findChild<QObject *>("reviewTrimArea")) QMetaObject::invokeMethod(area, "canceled");
                QTimer::singleShot(0, &app, *run);
            }
            else if (st.startsWith("trimstate=")) {
                auto *area = window->findChild<QObject *>("reviewTrimArea");
                if (!area) { app.exit(6); return; }
                area->setProperty("activeTime", 1.1); area->setProperty("mode", st.mid(10).toInt());
                QTimer::singleShot(0, &app, *run);
            }
            else if (st.startsWith("timelinepress=") || st.startsWith("timelinemove=") || st == "timelinerelease") {
                auto *bar = window->findChild<QQuickItem *>("reviewTimeline");
                if (!bar || review.duration() <= 0) { app.exit(6); return; }
                const double t = st.mid(st.indexOf('=') + 1).toDouble();
                const auto point = st == "timelinerelease" ? *mousePoint : bar->mapToScene(QPointF(bar->width() * t / review.duration(), 48));
                *mousePoint = point;
                const bool move = st.startsWith("timelinemove="), release = st == "timelinerelease";
                QMouseEvent event(move ? QEvent::MouseMove : release ? QEvent::MouseButtonRelease : QEvent::MouseButtonPress,
                    point, window->mapToGlobal(point.toPoint()), move ? Qt::NoButton : Qt::LeftButton,
                    release ? Qt::NoButton : Qt::LeftButton, Qt::NoModifier);
                event.setTimestamp(QDateTime::currentMSecsSinceEpoch());
                QCoreApplication::sendEvent(window, &event); QTimer::singleShot(0, &app, *run);
            }
            else if (st.startsWith("foldertext=")) {
                if (auto *input = window->findChild<QObject *>("folderInput")) input->setProperty("text", st.mid(11));
                QTimer::singleShot(0, &app, *run);
            }
            else if (st == "applyfolder") {
                if (auto *button = window->findChild<QObject *>("applyFolder")) QMetaObject::invokeMethod(button, "clicked");
                QTimer::singleShot(0, &app, *run);
            }
            else if (st == "focusfolder") {
                if (auto *input = window->findChild<QQuickItem *>("folderInput")) input->forceActiveFocus();
                QTimer::singleShot(0, &app, *run);
            }
            else if (st.startsWith("click=")) {
                auto *button = window->findChild<QQuickItem *>(st.mid(6));
                if (!button || !button->isVisible() || !button->isEnabled() || !QMetaObject::invokeMethod(button, "clicked")) { app.exit(4); return; }
                QTimer::singleShot(0, &app, *run);
            }
            else if (st == "closewindow") { window->close(); QTimer::singleShot(0, &app, *run); }
            else if (st == "forcequit") {
                auto *button = window->findChild<QQuickItem *>("forceQuitButton");
                if (!button || !button->isVisible()) { app.exit(4); return; }
                QMetaObject::invokeMethod(button, "clicked");
                app.exit(4); // a confirmed forced exit must not return
            }
            else if (st.startsWith("expect=")) {
                const auto condition = st.mid(7).split(':');
                bool ok = condition.size() == 2;
                if (ok && condition[0] == "state") ok = recorder.state() == condition[1];
                else if (ok && condition[0] == "pro") ok = state.pro() == (condition[1] == "1");
                else if (ok && condition[0] == "screen") ok = state.screen() == condition[1].toInt();
                else if (ok && condition[0] == "cameraName") ok = state.cameraName() == condition[1];
                else if (ok && condition[0] == "folderDefault") ok = prefs.folderIsDefault() == (condition[1] == "1");
                else if (ok && condition[0] == "unsaved") ok = review.unsaved() == (condition[1] == "1");
                else if (ok && condition[0] == "loading") ok = review.loading() == (condition[1] == "1");
                else if (ok && condition[0] == "trimMode") { auto *area = window->findChild<QObject *>("reviewTrimArea"); ok = area && area->property("mode").toInt() == condition[1].toInt(); }
                else if (ok && condition[0] == "canUndo") ok = review.canUndo() == (condition[1] == "1");
                else if (ok && condition[0] == "audioProblem") ok = recorder.audioProblem().contains(condition[1]);
                else if (ok && condition[0] == "audioProblemEmpty") ok = recorder.audioProblem().isEmpty() == (condition[1] == "1");
                else if (ok && condition[0] == "notice") { auto *notice = window->findChild<QObject *>("readyNotice"); ok = notice && notice->property("text").toString().contains(condition[1]); }
                else if (ok && condition[0] == "reviewNotice") { auto *notice = window->findChild<QObject *>("reviewNotice"); ok = notice && notice->property("text").toString().contains(condition[1]); }
                else if (ok && condition[0] == "previewFrame") ok = preview.hasFrame() == (condition[1] == "1");
                else if (ok && condition[0] == "preview") ok = state.preview() == (condition[1] == "1");
                else if (ok && condition[0] == "previewing") ok = recorder.previewing() == (condition[1] == "1");
                else if (ok && condition[0] == "message") ok = recorder.message().contains(condition[1]);
                else if (ok && condition[0] == "armed") ok = recorder.streamArmed() == (condition[1] == "1");
                else if (ok && condition[0] == "broadcasting") ok = recorder.broadcasting() == (condition[1] == "1");
                else if (ok && condition[0] == "streamState") ok = recorder.streamState() == condition[1];
                else if (ok && condition[0] == "streamReady") ok = prefs.streamReady() == (condition[1] == "1");
                else if (ok && condition[0] == "streamProblem") ok = recorder.streamProblem().contains(condition[1]);
                else if (ok && condition[0] == "messageEmpty") ok = recorder.message().isEmpty() == (condition[1] == "1");
                else if (ok && condition[0] == "audioTab") ok = window->findChild<QObject *>("expertPanel")->property("tab").toInt() == condition[1].toInt();
                else if (ok && condition[0] == "splashDone") { auto *splash = window->findChild<QObject *>("splash"); ok = splash && splash->property("done").toBool() == (condition[1] == "1"); }
                else if (ok && condition[0] == "confirmQuit") ok = window->property("confirmQuit").toBool() == (condition[1] == "1");
                else if (ok && condition[0] == "confirmStopQuit") ok = window->property("confirmStopQuit").toBool() == (condition[1] == "1");
                else ok = false;
                if (!ok) { std::fprintf(stderr, "script: assertion failed %s\n", qPrintable(st)); app.exit(4); return; }
                QTimer::singleShot(0, &app, *run);
            }
            else if (st == "checklayout") {
                if (!qmlWarnings.isEmpty()) { app.exit(6); return; }
                QStringList problems;
                std::function<void(QQuickItem *, QRectF)> inspect = [&](QQuickItem *item, QRectF area) {
                    if (!item->isVisible()) return;
                    if (item->objectName() == "contentScreen" || item->objectName() == "expertPanel")
                        area = item->mapRectToItem(window->contentItem(), QRectF(0, 0, item->width(), item->height()));
                    const QString text = item->property("text").toString();
                    if (item->objectName() == "livePreviewCaption") {
                        auto *button = window->findChild<QQuickItem *>("recordButton");
                        if (button && button->isVisible() && item->mapRectToItem(window->contentItem(), item->boundingRect()).intersects(button->mapRectToItem(window->contentItem(), button->boundingRect())))
                            problems.append("live preview overlaps Record");
                    }
                    if ((!text.isEmpty() && !item->inherits("QQuickTextInput")) || item->objectName() == "recordButton" || item->objectName() == "readyPreview" || item->objectName() == "recordingPreview" || item->objectName() == "liveBadge") {
                        const QRectF box = item->mapRectToItem(window->contentItem(), QRectF(0, 0, item->width(), item->height()));
                        if (box.left() < area.left() - 1 || box.top() < area.top() - 1 || box.right() > area.right() + 1 || box.bottom() > area.bottom() + 1)
                            problems.append(text.isEmpty() ? item->objectName() : text.left(100));
                    }
                    for (auto *child : item->childItems()) inspect(child, area);
                };
                inspect(window->contentItem(), QRectF(0, 0, window->width(), window->height()));
                if (!problems.isEmpty()) { std::fprintf(stderr, "layout overflow: %s\n", qPrintable(problems.join(" | "))); app.exit(5); return; }
                QTimer::singleShot(0, &app, *run);
            }
            else if (st.startsWith("micdevice=")) { prefs.setMicDevice(st.mid(10)); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("micgain=")) { prefs.setMicGainDb(st.mid(8).toInt()); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("desktop=")) { prefs.setDesktopOn(st.mid(8) == "1"); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("miccheck=")) { recorder.checkMic(st.mid(9) == "1"); QTimer::singleShot(0, &app, *run); }
            else if (st == "micfailure" && dynamic_cast<ScriptBackend *>(backend.get())) { backend->setMicCheck(false, AudioSpec{}); emit backend->micCheckFailed("microphone capture failed"); QTimer::singleShot(0, &app, *run); }
            else if (st == "previewfailure" && dynamic_cast<ScriptBackend *>(backend.get())) { emit backend->previewFailed("preview capture failed"); QTimer::singleShot(0, &app, *run); }
            else if (st == "hardwarecheck") { recorder.checkHardware(); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("hardware=")) { prefs.setHardware(st.mid(9) == "1"); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("quality=")) { prefs.setQuality(st.mid(8).toInt()); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("arm=")) { recorder.setStreamArmed(st.mid(4) == "1"); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("streamurl=")) { const QString why = prefs.setStreamUrl(st.mid(10)); if (!why.isEmpty()) std::fprintf(stderr, "script: %s\n", qPrintable(why)); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("streamkey=")) { prefs.setStreamKey(st.mid(10)); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("streamquality=")) { prefs.setStreamQuality(st.mid(14).toInt()); QTimer::singleShot(0, &app, *run); }
            else if (st == "streamdrop" && dynamic_cast<ScriptBackend *>(backend.get())) { emit backend->streamState("failed", "the connection to the server was lost"); QTimer::singleShot(0, &app, *run); }
            else if (st == "streamretry" && dynamic_cast<ScriptBackend *>(backend.get())) { emit backend->streamState("reconnecting", "the connection dropped: trying again"); QTimer::singleShot(0, &app, *run); }
            else if (st == "streamback" && dynamic_cast<ScriptBackend *>(backend.get())) { emit backend->streamState("live", QString()); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("mic=")) { prefs.setMicOn(st.mid(4) == "1"); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("review=")) { std::printf("script: review %d\n", int(review.open(st.mid(7)))); waitFor([&] { return !review.loading(); }, 35000, "the recording to open"); }
            else if (st.startsWith("savedtake=") && dynamic_cast<ScriptBackend *>(backend.get())) { emit backend->finished(st.mid(10)); QTimer::singleShot(0, &app, *run); }
            else if (st == "waitwarning") waitFor([&] { return !recorder.warning().isEmpty(); }, 5000, "picture warning");
            else if (st.startsWith("beginreview=")) { review.open(st.mid(12)); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("split=")) { review.split(st.mid(6).toDouble()); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("cut=")) { review.removeClip(st.mid(4).toInt()); QTimer::singleShot(0, &app, *run); }
            else if (st == "savereview") { review.save(); QTimer::singleShot(0, &app, *run); }
            else if (st == "waitsaved") waitFor([&] { return !review.saving(); }, 60000, "the edit to be saved");
            else if (st == "closereview") { review.close(); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("preview=")) { state.setPreview(st.mid(8) == "1"); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("panel=")) { state.setCameraPanel(st.mid(6) == "1"); QTimer::singleShot(0, &app, *run); }
            else if (st == "quit") app.exit(0); // the test runner explicitly bypasses window-close confirmation
            else { qWarning() << "script: unknown step" << st; app.exit(3); }
        };
        QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [run] { *run = {}; });
        QTimer::singleShot(1500, &app, *run);   // let the window appear first
    }
    return app.exec();
}
