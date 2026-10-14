#include "appstate.h"
#include "audiodevices.h"
#include "control.h"
#include "engine.h"
#include "mobslog.h"
#include "obsbackend.h"
#include "palette.h"
#include "notifier.h"
#include "pipcontroller.h"
#include "previewmodel.h"
#include "review.h"
#include "settings.h"
#include "thumbprovider.h"
#include <QDateTime>
#include <QFile>
#include <QQmlContext>
#include <QDir>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QClipboard>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <QKeyEvent>
#include <QSettings>
#include <QTimer>
#include <QDebug>
#include <functional>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <QtQml/qqml.h>
#include "chatmodel.h"
#include "chatrelay.h"
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

// Development-script fixture for rendering active UI states offscreen. It never creates media.
// Both MOBS_SCRIPT and MOBS_FAKE_BACKEND must be explicit; ordinary launches always use libobs.
class ScriptBackend final : public RecorderBackend {
public:
    void begin(const RecordSpec &spec) override {
        m_streams = spec.stream.on;
        emit started(); emit audioLevels(-18, -24, spec.mic, spec.audio.desktop, QString());
        if (m_streams) {   // a pretend broadcast: connecting first, live a moment later (the steps below then break it on purpose)
            emit streamState("connecting", QString());
            if (qEnvironmentVariable("MOBS_FAKE_STREAM_NEVER_LIVE") != "1")   // =1: the server never answers, as with a wrong key or no network
                QTimer::singleShot(400, this, [this] { if (m_streams) { emit streamState("live", QString()); emit streamStats(9000, 0); } });
        }
    }
    void pause(bool) override {}
    void end() override {
        if (qEnvironmentVariable("MOBS_FAKE_STOP_HANG") == "1") return;
        if (m_streams) { m_streams = false; emit streamState("ended", QString()); }
        emit audioLevels(-60, -60, false, false, QString()); emit finished(QString());
    }
    bool m_streams = false;
    ScriptBackend() {
        m_frames.setInterval(100);
        connect(&m_frames, &QTimer::timeout, this, [this] { QImage f(640, 360, QImage::Format_RGB32); f.fill(m_black ? QColor(Qt::black) : QColor("#3050f0")); emit previewFrame(f); });
    }
    // blackpicture=1: the screen source is up and delivers nothing but black, frame after frame, as a broken screen share does. 0: a real picture again.
    void showBlack(bool black) { m_black = black; m_framesOn = true; emit captureSized(1920, 1080); m_frames.start(); }
    bool m_black = false, m_framesOn = false;
    QTimer m_frames;
    void setMicCheck(bool on, const AudioSpec &) override { emit audioLevels(on ? -18 : -60, -60, on, false, QString()); }
    void setPreview(bool on, const RecordSpec &) override {
        if (!on) { m_frames.stop(); return; }
        if (m_framesOn) m_frames.start();
        // MOBS_FAKE_PREVIEW=/path.png serves that picture instead of the flat colour, so the real window can be drawn with a mock-up picture inside.
        const QString fakePicture = qEnvironmentVariable("MOBS_FAKE_PREVIEW");
        QImage frame = fakePicture.isEmpty() ? QImage() : QImage(fakePicture);
        if (frame.isNull()) { frame = QImage(640, 360, QImage::Format_RGB32); frame.fill(QColor("#3050f0")); }
        else frame = frame.scaled(640, 360, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB32);
        if (m_framesOn && m_black) frame.fill(Qt::black);   // a broken share stays black whenever the preview is asked for again (arming asks)
        emit previewFrame(frame);
    }
};

int main(int argc, char *argv[]) {
    qputenv("QT_MEDIA_BACKEND", "ffmpeg");   // Qt's ffmpeg multimedia backend plays the takes in the review screen
    QtWebEngineQuick::initialize();          // QtWebEngine must be set up before the application object exists; kept for a later feature, it starts nothing by itself
    QGuiApplication app(argc, argv);
    app.setApplicationName("mobs");
    app.setOrganizationName("mobs");
    app.setDesktopFileName("mobs");
    QQuickWindow::setDefaultAlphaBuffer(true);   // the webcam dock is see-through between its cards once a chat joins: a surface made opaque cannot become see-through later
    MobsLog::init();   // first, so Qt's own warnings and everything below land in ~/.local/state/mobs/mobs.log

    // One copy at a time. A second copy finds the webcam held by the first, takes and streams with the same key (X drops one of the two)
    // and the stop hotkey reaches only the first: on 2026-10-12 three live takes ran at once, windows hidden, the webcam dead in all.
    // Tests that run several copies set MOBS_NO_HOTKEY=1. (Two launches within the same few milliseconds can still both pass; the later one
    // then fails to take the bus name and warns.)
    if (qEnvironmentVariable("MOBS_NO_HOTKEY") != "1") {
        if (const QString other = Control::runningCopy(); !other.isEmpty()) {
            MobsLog::write("start", QString("another MOBS is already running (%1): this copy does not start").arg(other));
            Notifier::send("MOBS is already running", Control::secondCopyNotice(other), true);
            return 0;
        }
    }

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
    {   // what a log reader needs first: what this is running on, and what MOBS found there
        QStringList screens, cameras;
        for (QScreen *sc : app.screens()) screens << QString("%1 %2x%3 at %4,%5 scale %6").arg(sc->name()).arg(sc->geometry().width()).arg(sc->geometry().height()).arg(sc->geometry().x()).arg(sc->geometry().y()).arg(sc->devicePixelRatio());
        for (const QVariant &c : state.cameras()) cameras << c.toMap().value("name").toString();
        MobsLog::write("start", QString("%1 | platform %2 | session %3").arg(prefs.aboutText().replace('\n', " | "), QGuiApplication::platformName(), qEnvironmentVariable("XDG_SESSION_TYPE", "?")));
        MobsLog::write("start", "screens: " + (screens.isEmpty() ? QString("none") : screens.join("; ")));
        MobsLog::write("start", "cameras: " + (cameras.isEmpty() ? QString("none") : cameras.join("; ")));
        MobsLog::write("start", QString("settings: pick=%1 preview=%2 hardware=%3 desktopSound=%4 mic=%5 streamReady=%6 streamQuality=%7 folder=%8")
            .arg(state.pick()).arg(state.preview()).arg(prefs.hardware()).arg(prefs.desktopOn()).arg(prefs.micOn()).arg(prefs.streamReady()).arg(prefs.streamQuality()).arg(prefs.folder()));
    }
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
    recorder.setCameraProvider([&] { auto c = state.cameraSpec(); c.pip = prefs.pip(); return c; });   // "screen + me" with the webcam as a window on the screen
    QObject::connect(&recorder, &Engine::cameraNeeded, &state, [&] { state.refreshCameras(); prefs.setSetupStep(1); state.openSetup(); });   // the guide's camera page lists the cameras and says what to do
    QObject::connect(&recorder, &Engine::liveSetupNeeded, &state, [&] { prefs.setSetupStep(5); state.openSetup(); });   // the guide walks the person from "open X" to the stream key
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
    // Armed to go live ALWAYS shows it: what is about to be broadcast must be seen first, whatever the preview chip says.
    auto syncPreview = [&] { recorder.setPreview((state.previewWanted() || recorder.streamArmed()) && state.screen() != 1, state.pick()); };
    QObject::connect(&recorder, &Engine::streamChanged, &state, [&] { syncPreview(); });
    QObject::connect(backend.get(), &RecorderBackend::previewFailed, &state, [&] { state.setPreview(false); });
    for (auto sig : {&AppState::screenChanged, &AppState::previewChanged, &AppState::pickChanged, &AppState::cameraChanged, &AppState::setupChanged})
        QObject::connect(&state, sig, &state, syncPreview);
    QObject::connect(&prefs, &Settings::changed, &state, syncPreview);   // the webcam window on or off rebuilds the picture (the backend ignores a repeat)
    ChatModel chat;   // the live chat beside the webcam, read-only: a feed fills it (X's relay later; the labelled demo only on request)
    PipController pip(&state, &recorder, &prefs, &settings);
    pip.setChat(&chat);
    control.setKillHandler([&pip] { pip.kill(); });   // the D-Bus door has a kill switch too (and the hotkey that stops a broadcast ends the livestream on X as well)
    // The livestreams whose chat was attached in earlier broadcasts are remembered (the last 20), so an old address that is still on the clipboard is never taken for a new one.
    chat.setUsedIds(settings.value("xUsedIds").toStringList());
    QObject::connect(&chat, &ChatModel::attached, &chat, [&settings, &chat](const QString &id) {
        QStringList ids = settings.value("xUsedIds").toStringList();
        ids.removeAll(id); ids.prepend(id);
        while (ids.size() > 20) ids.removeLast();
        settings.setValue("xUsedIds", ids); settings.sync();
        chat.setUsedIds(ids);
    });
    // A livestream's chat belongs to that livestream. While the livestream is being ended the chat page is the one that does it, so it stays until X has answered.
    QObject::connect(&recorder, &Engine::stateChanged, &chat, [&] { if (recorder.state() == "idle" && !chat.ending()) chat.detachX(); });
    QObject::connect(&chat, &ChatModel::endingChanged, &chat, [&] { if (!chat.ending() && recorder.state() == "idle") chat.detachX(); });
    // Stopping the video does not end the livestream on X: it stays open, public, with viewers watching a dead stream, until it is ended in Live Studio (found by Fred's first
    // real go-live: 62 viewers on a stream that had stopped). So when a take that was broadcasting ends, however it ends (stopped, or failed and went straight to idle), say so
    // and open the livestream in Live Studio: the livestream itself if MOBS found it, else the list of livestreams. The chat is detached when the take ends, so the id is kept here.
    // Only when X actually received the stream during the take (it reached "live"): a broadcast that never connected (a wrong key, no network) left nothing open on X, and
    // saying "viewers see a dead stream" then, and opening a browser, would be wrong. Without a livestream id the wording is conditional: the person may not have pressed Go Live.
    auto broadcastTake = std::make_shared<bool>(false);
    auto wentLive = std::make_shared<bool>(false);
    auto liveId = std::make_shared<QString>();
    QObject::connect(&chat, &ChatModel::statusChanged, &state, [&, liveId] { if (!chat.xId().isEmpty()) *liveId = chat.xId(); });
    QObject::connect(&recorder, &Engine::streamChanged, &state, [&, wentLive] { if (recorder.streamState() == "live") *wentLive = true; });
    // What to say when a broadcast is over. If the livestream on X was ended (the kill switch, or a stop of the broadcast), say so. If it could not be (no chat page to do it, or X did
    // not answer), say that it stays open and open it, as before.
    auto finishBroadcast = [&state, &chat, liveId](bool ended) {
        if (ended) {
            Notifier::send("MOBS: your livestream is ended", "The video has stopped and the livestream on X is ended. Viewers can see the replay.", true);
            MobsLog::write("app", "the broadcast is over and its livestream on X is ended");
        } else {
            Notifier::send("MOBS: your livestream is still open on X",
                           liveId->isEmpty() ? "The video has stopped. If you pressed Go Live in X, that livestream stays open until you press End livestream in Live Studio, and until then viewers see a dead stream."
                                             : "The video has stopped, but the livestream stays open on X until you press End livestream in Live Studio. Until then viewers see a dead stream.", true);
            state.openUrl(liveId->isEmpty() ? QStringLiteral("https://studio.x.com/live") : "https://studio.x.com/live/" + *liveId);
        }
        liveId->clear();
        Q_UNUSED(chat);
    };
    auto endNotice = std::make_shared<bool>(false);   // a broadcast is over and the notice waits for X's answer about ending the livestream
    QObject::connect(&chat, &ChatModel::endingChanged, &state, [&chat, endNotice, finishBroadcast] {
        if (chat.ending() || !*endNotice) return;
        *endNotice = false;
        finishBroadcast(chat.endedOk());
    });
    QObject::connect(&recorder, &Engine::stateChanged, &state, [&, broadcastTake, wentLive, liveId, endNotice, finishBroadcast] {
        const QString now = recorder.state();
        const bool taking = now == "starting" || now == "recording" || now == "paused";
        if (taking && recorder.streamRequested()) {
            if (now == "starting") chat.rememberClipboard();   // a livestream link that is already on the clipboard now is an old one: never picked up as this broadcast's
            *broadcastTake = true;
            return;
        }
        if (taking) return;
        const bool reached = *wentLive;
        *wentLive = false;
        if (!*broadcastTake) return;
        *broadcastTake = false;
        if (!reached) { liveId->clear(); MobsLog::write("app", "the broadcast ended without X ever receiving the stream: nothing is open on X, so no notice"); return; }
        if (chat.ending()) { *endNotice = true; return; }   // the livestream is being ended: the notice comes when X has answered
        finishBroadcast(chat.endedOk());
    });
    // The real chat, when asked for (INI chatRelay=true, or MOBS_CHAT_RELAY_PORT in a test): a script in the person's own browser on another computer
    // POSTs what his viewers say, over the tailnet, and the card under the webcam shows it. Off by default: a program that listens is a decision.
    ChatRelay relay(&chat);
    if (prefs.chatRelay() || !qEnvironmentVariable("MOBS_CHAT_RELAY_PORT").isEmpty()) {
        const QString hostText = qEnvironmentVariable("MOBS_CHAT_RELAY_HOST");
        QHostAddress host = hostText.isEmpty() ? ChatRelay::tailnetAddress() : QHostAddress(hostText);
        if (host.isNull()) { MobsLog::write("chat", "no tailnet address found (is tailscale up?): the chat relay listens on this computer only"); host = QHostAddress::LocalHost; }
        bool portGiven = false;
        const int envPort = qEnvironmentVariable("MOBS_CHAT_RELAY_PORT").toInt(&portGiven);
        relay.start(host, quint16(portGiven ? qBound(0, envPort, 65535) : prefs.chatRelayPort()));
    }
    QObject::connect(backend.get(), &RecorderBackend::captureSized, &pip, &PipController::setCaptureSize);   // which monitor is in the picture
    // A person working in another window needs to be TOLD: connected (and the one step left for them), dropped, not live, saved. Each state once.
    QObject::connect(&recorder, &Engine::streamChanged, &state, [&, last = QString("off")]() mutable {
        const QString now = recorder.streamState();
        if (now == last) return;
        const QString before = last; last = now;
        if (now == "live") Notifier::send("MOBS: X is receiving your stream", "Viewers see it once you press Go live in X Live Studio (studio.x.com/live).");
        else if (now == "reconnecting" && before == "live") Notifier::send("MOBS: the connection to X dropped", "Trying again. The recording is still being saved.");
        else if (now == "failed") Notifier::send("MOBS: NOT LIVE", recorder.streamProblem().left(220) + ". The recording is still being saved.", true);
    });
    QObject::connect(&recorder, &Engine::saved, &state, [](const QString &path) { Notifier::send("MOBS: take saved", path); });
    // A picture that turns black while people watch is the one thing worth interrupting for.
    QObject::connect(&recorder, &Engine::pictureChanged, &state, [&] {
        if (!recorder.pictureProblem().isEmpty() && recorder.active())
            Notifier::send("MOBS: the picture is BLACK", recorder.broadcasting() ? "Viewers on X see black. Stop, then press the screen button and choose again." : "This recording is black. Stop, then press the screen button and choose again.", true);
    });
    if (const int ms = qEnvironmentVariableIntValue("MOBS_BLACK_AFTER_MS"); ms > 0) recorder.setBlackAfter(ms);   // tests shorten how long black must last
    // MOBS_FAKE_CHAT=1: a made-up chat flow, labelled DEMO in the panel, so the dock can be seen and tried without X. Never on by default.
    if (qEnvironmentVariable("MOBS_FAKE_CHAT") == "1") {
        chat.setConfigured(true, true); chat.setStatus(true, QString());
        static const QStringList names{"sunny", "pixelpete", "dana", "tuxedo", "kiwi", "marlow"};
        static const QStringList lines{"love this setup", "what theme is that?", "can you show the terminal for a sec?", "the sound is crisp", "first time here, hi everyone",
                                       "that was smooth", "how long did that take to build?", "ok that is actually really cool"};
        auto *demo = new QTimer(&app);
        QObject::connect(demo, &QTimer::timeout, &chat, [&chat, n = 0]() mutable { const int k = ++n; chat.add(QString::number(k), names.at(k % names.size()), lines.at((k * 3) % lines.size())); });
        demo->start(1400);
    }

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
    qmlRegisterSingletonInstance("MobsCore", 1, 0, "Pip", &pip);
    qmlRegisterSingletonInstance("MobsCore", 1, 0, "Chat", &chat);

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
    // The guided setup opens by itself until it has been finished once. A scripted run or a screen grab never opens it unless MOBS_SETUP=1
    // (the older checks all start from the ready screen), and MOBS_SETUP=0 keeps it away.
    {
        const QString setupSwitch = qEnvironmentVariable("MOBS_SETUP");
        const bool automated = !qEnvironmentVariable("MOBS_SCRIPT").isEmpty() || !qEnvironmentVariable("MOBS_GRAB").isEmpty();
        if (setupSwitch == "1" || (setupSwitch != "0" && !automated && !prefs.setupDone())) state.openSetup();
    }
    const QString splashSwitch = qEnvironmentVariable("MOBS_SPLASH");
    const bool splashAtSet = qEnvironmentVariableIsSet("MOBS_SPLASH_AT");
    engine.rootContext()->setContextProperty("splashAllowed", splashSwitch == "1" || splashAtSet ||
        (splashSwitch != "0" && qEnvironmentVariable("MOBS_SCRIPT").isEmpty() && qEnvironmentVariable("MOBS_GRAB").isEmpty()));
    engine.rootContext()->setContextProperty("splashAt", splashAtSet ? qEnvironmentVariableIntValue("MOBS_SPLASH_AT") : -1);
    // X, inside MOBS: where its cookies are kept, the page the person signs in on, and the page of a livestream (%1 = its id). Tests point the last two at stand-in files.
    engine.rootContext()->setContextProperty("xStoragePath", qEnvironmentVariable("MOBS_WEB_DIR", cfg + "/web"));
    engine.rootContext()->setContextProperty("xLoginUrl", qEnvironmentVariable("MOBS_X_LOGIN_URL", "https://x.com/home"));
    engine.rootContext()->setContextProperty("xChatUrlTemplate", qEnvironmentVariable("MOBS_X_CHAT_URL", "https://studio.x.com/live/%1"));   // the page itself: x.com/i/live-studio/<id> holds it in an iframe a script cannot see into
    engine.rootContext()->setContextProperty("xStudioUrl", qEnvironmentVariable("MOBS_X_STUDIO_URL", "https://studio.x.com/live"));   // the guide opens it once after sign-in, to see that Live Studio opens for this account
    // The go-live helper (LivePilot): it does the steps in Live Studio for the person. MOBS_NO_PILOT=1 turns it off (the window is then the steps for the person alone, as before),
    // MOBS_PILOT_TICK_MS is how often it looks at the page, and MOBS_PILOT_WAIT_MS how long it waits for X to say it sees the video before it gives up (tests shorten both).
    {   // the page half of the helper (a file in the resources): loaded into every Live Studio page the helper opens
        QFile helper(QStringLiteral(":/qt/qml/Mobs/assets/pilot/xstudio-pilot.js"));
        engine.rootContext()->setContextProperty("pilotHelperJs", helper.open(QIODevice::ReadOnly) ? QString::fromUtf8(helper.readAll()) : QString());
    }
    engine.rootContext()->setContextProperty("pipDefaultWidth", qBound(160, qEnvironmentVariableIntValue("MOBS_PIP_WIDTH") ?: 480, 3000));
    engine.rootContext()->setContextProperty("pilotAllowed", qEnvironmentVariable("MOBS_NO_PILOT") != "1");
    engine.rootContext()->setContextProperty("pilotTickMs", qEnvironmentVariableIntValue("MOBS_PILOT_TICK_MS") > 0 ? qEnvironmentVariableIntValue("MOBS_PILOT_TICK_MS") : 1000);
    engine.rootContext()->setContextProperty("pilotPatienceMs", qEnvironmentVariableIntValue("MOBS_PILOT_WAIT_MS") > 0 ? qEnvironmentVariableIntValue("MOBS_PILOT_WAIT_MS") : 100000);
    engine.rootContext()->setContextProperty("xSourcesUrl", qEnvironmentVariable("MOBS_X_SOURCES_URL", "https://studio.x.com/live/sources"));   // the Sources page: the setup guide reads the stream key and address from it
    engine.rootContext()->setContextProperty("xChatTries", qEnvironmentVariableIntValue("MOBS_X_TRIES") > 0 ? qEnvironmentVariableIntValue("MOBS_X_TRIES") : 12);   // tests shorten how long the chat card is looked for
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
        auto pipItem = [](const QString &name) -> QQuickItem * {   // the LAST showing item of that name in the webcam window (the newest chat row, say)
            QQuickWindow *pw = nullptr;
            for (QWindow *w : QGuiApplication::allWindows()) if (w->title() == PipController::kTitle && w->isVisible()) pw = qobject_cast<QQuickWindow *>(w);
            QQuickItem *hit = nullptr;
            std::function<void(QQuickItem *)> find = [&](QQuickItem *at) { if (!at->isVisible()) return; if (at->objectName() == name) hit = at; for (auto *child : at->childItems()) find(child); };
            if (pw) find(pw->contentItem());
            return hit;
        };
        *run = [=, &app, &recorder, &state, &preview, &review, &prefs, &qmlWarnings, &backend, &pip, &chat, &relay]() mutable {
            if (*idx >= steps->size()) return;
            const QString st = steps->at((*idx)++);
            std::printf("script: %s\n", qPrintable(st.startsWith("streamkey=") ? QStringLiteral("streamkey=<kept private>") : st)); std::fflush(stdout);   // the key is never printed, not even in a test hook's echo
            if (st == "start") { recorder.start(state.pick()); QTimer::singleShot(0, &app, *run); }
            else if (st == "waitrec") waitFor([&] { return recorder.state() == "recording"; }, 90000, "recording to begin");
            else if (st == "waitidle") waitFor([&] { return recorder.state() == "idle"; }, 30000, "the file to be saved");
            else if (st.startsWith("wait=")) QTimer::singleShot(int(st.mid(5).toDouble() * 1000), &app, *run);
            else if (st == "pause" || st == "resume") { recorder.togglePause(); QTimer::singleShot(0, &app, *run); }
            else if (st == "stop") { recorder.stop(); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("grab=")) { std::printf("script: grab %d\n", int(window->grabWindow().save(st.mid(5)))); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("pick=")) { state.choose(st.mid(5).toInt()); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("corner=")) { state.setCorner(st.mid(7).toInt()); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("movecam=")) { const auto xy = st.mid(8).split(':'); if (xy.size() == 2) state.moveCamera(xy[0].toDouble(), xy[1].toDouble(), true); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("dragcam=")) {   // press on the real handle, move by DX:DY window pixels, release: what a hand does
                const auto d = st.mid(8).split(':');
                // The ready screen and the recording screen each have a picture with a handle: take the one that is showing.
                std::function<QQuickItem *(QQuickItem *)> find = [&](QQuickItem *at) -> QQuickItem * {
                    if (at->objectName() == "webcamHandle" && at->isVisible()) return at;
                    for (auto *child : at->childItems()) if (auto *hit = find(child)) return hit;
                    return nullptr;
                };
                auto *handle = find(window->contentItem());
                if (!handle || !handle->isVisible() || d.size() != 2) { std::fprintf(stderr, "script: no webcam handle to drag\n"); app.exit(4); return; }
                const QPointF start = handle->mapToScene(QPointF(handle->width() / 2, handle->height() / 2)), end = start + QPointF(d[0].toDouble(), d[1].toDouble());
                const auto send = [&](QEvent::Type type, QPointF at, Qt::MouseButtons buttons) {
                    QMouseEvent e(type, at, window->mapToGlobal(at), type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, buttons, Qt::NoModifier);
                    QCoreApplication::sendEvent(window, &e);
                };
                send(QEvent::MouseButtonPress, start, Qt::LeftButton);
                for (int i = 1; i <= 6; ++i) send(QEvent::MouseMove, start + (end - start) * (i / 6.0), Qt::LeftButton);
                send(QEvent::MouseButtonRelease, end, Qt::NoButton);
                QTimer::singleShot(0, &app, *run);
            }
            else if (st.startsWith("grabpip=")) {   // the webcam window's own pixels (its badge, in every state), for design checks
                QQuickWindow *pipWindow = nullptr;
                for (QWindow *w : QGuiApplication::allWindows()) if (w->title() == PipController::kTitle && w->isVisible()) pipWindow = qobject_cast<QQuickWindow *>(w);
                if (!pipWindow) { std::fprintf(stderr, "script: no webcam window to grab\n"); app.exit(4); return; }
                std::printf("script: grabpip %d\n", int(pipWindow->grabWindow().save(st.mid(8)))); QTimer::singleShot(0, &app, *run);
            }
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
                const int key = name == "Space" ? Qt::Key_Space : name == "Esc" ? Qt::Key_Escape : name == "P" ? Qt::Key_P : name == "3" ? Qt::Key_3 : name == "4" ? Qt::Key_4 : Qt::Key_unknown;
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
                // Items a Repeater makes are not QObject children of the window, so findChild misses them: walk the visual tree too.
                std::function<QQuickItem *(QQuickItem *, const QString &)> findVisual = [&](QQuickItem *at, const QString &name) -> QQuickItem * {
                    if (at->objectName() == name) return at;
                    for (auto *child : at->childItems()) if (auto *hit = findVisual(child, name)) return hit;
                    return nullptr;
                };
                auto *button = window->findChild<QQuickItem *>(st.mid(6));
                if (!button) button = findVisual(window->contentItem(), st.mid(6));
                if (!button || !button->isVisible() || !button->isEnabled() || !QMetaObject::invokeMethod(button, "clicked")) {
                    std::fprintf(stderr, "script: cannot click %s (found %d, visible %d, enabled %d)\n", qPrintable(st.mid(6)), int(button != nullptr), int(button && button->isVisible()), int(button && button->isEnabled()));
                    app.exit(4); return;
                }
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
                else if (ok && condition[0] == "pipWanted") ok = pip.wanted() == (condition[1] == "1");
                else if (ok && condition[0] == "pipShown") { bool shown = false; for (auto *w : QGuiApplication::allWindows()) if (w->title() == PipController::kTitle && w->isVisible()) shown = true; ok = shown == (condition[1] == "1"); }
                else if (ok && condition[0] == "camMoved") ok = (state.cameraX() >= 0) == (condition[1] == "1");
                else if (ok && condition[0] == "armed") ok = recorder.streamArmed() == (condition[1] == "1");
                else if (ok && condition[0] == "broadcasting") ok = recorder.broadcasting() == (condition[1] == "1");
                else if (ok && condition[0] == "streamState") ok = recorder.streamState() == condition[1];
                else if (ok && condition[0] == "streamReady") ok = prefs.streamReady() == (condition[1] == "1");
                else if (ok && condition[0] == "streamProblem") ok = recorder.streamProblem().contains(condition[1]);
                else if (ok && condition[0] == "pictureProblem") ok = recorder.pictureProblem().contains(condition[1]);
                else if (ok && condition[0] == "pictureProblemEmpty") ok = recorder.pictureProblem().isEmpty() == (condition[1] == "1");
                else if (ok && condition[0] == "chatCount") ok = chat.count() == condition[1].toInt();
                else if (ok && condition[0] == "chatConnected") ok = chat.connected() == (condition[1] == "1");   // the other computer's script has called lately
                else if (ok && condition[0] == "setupOpen") ok = state.setupOpen() == (condition[1] == "1");   // the guided setup is showing
                else if (ok && condition[0] == "mainHeld") ok = pip.mainHeld() == (condition[1] == "1");
                else if (ok && condition[0] == "chatXId") ok = chat.xId() == condition[1];
                else if (ok && condition[0] == "finderNote") ok = condition[1] == "none" ? chat.finderNote().isEmpty() : chat.finderNote().startsWith(condition[1]);   // finderNote:TEXT: what the livestream finder tells the person begins like this; finderNote:none: nothing
                else if (ok && condition[0] == "xSignedIn") ok = prefs.xSignedIn() == (condition[1] == "1");
                else if (ok && condition[0] == "xCrop") { QQuickItem *view = pipItem("xChatView"); ok = view && view->property("cropState").toString() == condition[1]; }
                else if (ok && condition[0] == "xRegion") { QQuickItem *view = pipItem("xChatView"); ok = view && QString("%1x%2").arg(qRound(view->property("regionW").toDouble())).arg(qRound(view->property("regionH").toDouble())) == condition[1]; }   // the part of X's page that is shown, in the page's own pixels
                else if (ok && condition[0] == "xRegionTall") { QQuickItem *view = pipItem("xChatView"); const QStringList range = condition[1].split('-'); const double h = view ? view->property("regionH").toDouble() : -1; ok = view && range.size() == 2 && h >= range[0].toDouble() && h <= range[1].toDouble(); }   // xRegionTall:MIN-MAX: the shown part of the card is that tall, in page pixels
                else if (ok && condition[0] == "xFresh") { QQuickItem *view = pipItem("xChatView"); const int beats = view ? view->property("freshBeats").toInt() : -1; ok = view && (condition[1] == "none" ? beats == 0 : beats >= condition[1].toInt()); }   // xFresh:N: the card has asked Chromium for a fresh frame at least N times; xFresh:none: not once (it only does while the card is shown)
                else if (ok && condition[0] == "xPixels") {   // xPixels:RRGGBB=some|none: does the chat card show a patch of that colour? Proves that only the Live Chat card is on screen
                    QQuickItem *view = pipItem("xChatView");
                    ok = false;
                    if (view && view->window()) {
                        const QImage shot = view->window()->grabWindow().convertToFormat(QImage::Format_RGB32);
                        const qreal dpr = shot.devicePixelRatio();
                        const QRectF scene = view->mapRectToScene(QRectF(0, 0, view->width(), view->height()));
                        const QRect area = QRect(qRound(scene.x() * dpr), qRound(scene.y() * dpr), qRound(scene.width() * dpr), qRound(scene.height() * dpr)).intersected(shot.rect());
                        const QRgb want = QColor("#" + condition[1].section('=', 0, 0)).rgb();
                        int near = 0;
                        for (int y = area.top(); y <= area.bottom(); ++y) for (int x = area.left(); x <= area.right(); ++x) { const QRgb p = shot.pixel(x, y); if (qAbs(qRed(p) - qRed(want)) < 10 && qAbs(qGreen(p) - qGreen(want)) < 10 && qAbs(qBlue(p) - qBlue(want)) < 10) ++near; }
                        ok = condition[1].section('=', 1, 1) == "none" ? near == 0 : near > 400;
                        if (!ok) std::fprintf(stderr, "script: xPixels %s: %d matching pixels in %dx%d\n", qPrintable(condition[1]), near, area.width(), area.height());
                    } else std::fprintf(stderr, "script: xPixels: no chat view or window (view %p)\n", static_cast<void *>(view));
                }   // MOBS's window is kept up while the person finishes going live
                else if (ok && condition[0] == "setupStep") ok = prefs.setupStep() == condition[1].toInt();
                else if (ok && condition[0] == "sourceName") ok = prefs.sourceName() == condition[1];
                else if (ok && condition[0] == "setupMicAllShown") { auto *list = window->findChild<QObject *>("setupMicList"); ok = list && list->property("allShown").toBool() == (condition[1] == "1"); }   // every microphone is in view (else the bar beside the list shows)
                else if (ok && condition[0] == "setupMicRows") { auto *list = window->findChild<QObject *>("setupMicList"); ok = list && list->property("count").toInt() == condition[1].toInt(); }   // how many rows the microphone list has, "system default" included
                else if (ok && condition[0] == "setupMicFirst") { auto *list = window->findChild<QObject *>("setupMicList"); ok = list && list->property("firstRow").toString().contains(condition[1]); }   // the first row says what the system default is
                else if (ok && condition[0] == "setupCameraAllShown") { auto *list = window->findChild<QObject *>("setupCameraList"); ok = list && list->property("allShown").toBool() == (condition[1] == "1"); }
                else if (ok && condition[0] == "studioTitle") { auto *view = window->findChild<QObject *>("studioView"); ok = view && view->property("title").toString().contains(condition[1]); }   // what the page in the helper's box calls itself (the stand-in sign-in page writes what it saw there)
                else if (ok && condition[0] == "xMessages") { QQuickItem *view = pipItem("xChatView"); ok = view && view->property("messages").toInt() == condition[1].toInt(); }   // how many messages the chat card shows (0: nobody has written yet)
                else if (ok && condition[0] == "chatViewers") ok = chat.viewers() == condition[1].toInt();                 // people watching, as the page of the livestream says (-1 until it has)
                else if (ok && condition[0] == "chatTrend") ok = chat.viewerTrend() == condition[1].toInt();               // +1 up, -1 down, 0 steady or too early
                else if (ok && condition[0] == "chatDelta") ok = chat.viewerDelta() == condition[1].toInt();
                else if (ok && condition[0] == "chatTotal") ok = chat.totalViews() == condition[1].toInt();
                else if (ok && condition[0] == "chatEnding") ok = chat.ending() == (condition[1] == "1");                    // the livestream is being ended on X
                else if (ok && condition[0] == "chatEnded") ok = chat.endedOk() == (condition[1] == "1");                   // and the last end worked
                else if (ok && condition[0] == "pipText") { QQuickItem *item = pipItem(condition[1].section('=', 0, 0)); ok = item && item->property("text").toString().contains(condition[1].section('=', 1)); }   // pipText:NAME=TEXT: an item of the webcam window says this
                else if (ok && condition[0] == "uiScale") ok = qAbs(window->property("uiScale").toDouble() - condition[1].toDouble()) < 0.01;   // how far the interface is scaled up for this window (1 to 3.5)
                else if (ok && condition[0] == "uiCanvasOk") { auto *ui = window->findChild<QQuickItem *>("ui"); ok = ui && (ui->width() >= 859.9 && ui->height() >= 559.9) == (condition[1] == "1"); }   // the canvas the screens are laid out on is never smaller than 860 by 560
                else if (ok && condition[0] == "sourcePhase") { auto *pilot = window->findChild<QObject *>("sourcePilot"); ok = pilot ? pilot->property("phase").toString() == condition[1] : condition[1] == "none"; }   // what the setup page's helper is doing: loading, signin, looking, creating, reading, done, failed
                else if (ok && condition[0] == "streamUrlHas") ok = prefs.streamUrl().contains(condition[1]);   // the server address contains this (no colon: the expect syntax has only one)
                else if (ok && condition[0] == "streamKeyIs") ok = prefs.streamSpec().key == condition[1];   // tests use a made-up key: this compares it, C++ only, and never prints a real one
                else if (ok && condition[0] == "pilotPhase") { auto *pilot = window->findChild<QObject *>("livePilot"); ok = pilot ? pilot->property("phase").toString() == condition[1] : condition[1] == "none"; }   // what the go-live helper is doing: loading, opening, filling, waiting, pressing, done, failed (none: it is not running)
                else if (ok && condition[0] == "pilotProgress") { auto *pilot = window->findChild<QObject *>("livePilot"); ok = pilot && pilot->property("progress").toInt() == condition[1].toInt(); }
                else if (ok && condition[0] == "pilotNote") { auto *pilot = window->findChild<QObject *>("livePilot"); ok = pilot && pilot->property("note").toString().contains(condition[1]); }
                else if (ok && condition[0] == "pilotReason") { auto *pilot = window->findChild<QObject *>("livePilot"); ok = pilot && pilot->property("reason").toString().contains(condition[1]); }
                else if (ok && condition[0] == "pilotFailure") { auto *pilot = window->findChild<QObject *>("livePilot"); ok = pilot && pilot->property("failure").toString().contains(condition[1]); }
                else if (ok && condition[0] == "pilotTitle") { auto *pilot = window->findChild<QObject *>("livePilot"); ok = pilot && pilot->property("seenTitle").toString() == condition[1]; }   // the title the form showed when the helper last looked
                else if (ok && condition[0] == "pilotVisibility") { auto *pilot = window->findChild<QObject *>("livePilot"); ok = pilot && pilot->property("seenVisibility").toString() == condition[1]; }
                else if (ok && condition[0] == "pilotSource") { auto *pilot = window->findChild<QObject *>("livePilot"); ok = pilot && pilot->property("seenSource").toString() == condition[1]; }
                else if (ok && condition[0] == "readyTitle") { auto *box = window->findChild<QObject *>("liveTitleInput"); ok = box && box->property("text").toString() == condition[1]; }   // what the ready screen's title box shows
                else if (ok && condition[0] == "liveTitle") ok = prefs.liveTitleForX() == condition[1];
                else if (ok && condition[0] == "livePublic") ok = prefs.livePublic() == (condition[1] == "1");
                else if (ok && condition[0] == "goLiveSteps") { auto *steps = window->findChild<QObject *>("goLiveSteps"); ok = steps && steps->property("text").toString().contains(condition[1]); }   // the go-live window's steps say this
                else if (ok && condition[0] == "goLiveNote") { auto *line = window->findChild<QObject *>("goLiveChatStatus"); ok = line && line->property("text").toString().contains(condition[1]); }   // the go-live window's chat line says this
                else if (ok && condition[0] == "setupDone") ok = prefs.setupDone() == (condition[1] == "1");
                else if (ok && condition[0] == "cameraLocked") ok = state.cameraLocked() == (condition[1] == "1");
                else if (ok && condition[0] == "micLocked") ok = prefs.micLocked() == (condition[1] == "1");
                else if (ok && condition[0] == "pipPlain") { QQuickItem *item = pipItem(condition[1]); ok = item && item->property("textFormat").toInt() == int(Qt::PlainText); }   // a stranger's words are shown as plain text
                else if (ok && condition[0] == "chatWanted") ok = pip.chatWanted() == (condition[1] == "1");
                else if (ok && condition[0] == "pipHas") ok = pipItem(condition[1]) != nullptr;   // an item of that name is showing in the webcam window
                else if (ok && condition[0] == "pipGone") ok = pipItem(condition[1]) == nullptr;
                else if (ok && condition[0] == "messageEmpty") ok = recorder.message().isEmpty() == (condition[1] == "1");
                else if (ok && condition[0] == "audioTab") ok = window->findChild<QObject *>("expertPanel")->property("tab").toInt() == condition[1].toInt();
                else if (ok && condition[0] == "splashDone") { auto *splash = window->findChild<QObject *>("splash"); ok = splash && splash->property("done").toBool() == (condition[1] == "1"); }
                else if (ok && condition[0] == "confirmQuit") ok = window->property("confirmQuit").toBool() == (condition[1] == "1");
                else if (ok && condition[0] == "confirmStopQuit") ok = window->property("confirmStopQuit").toBool() == (condition[1] == "1");
                else ok = false;
                if (!ok) { std::fprintf(stderr, "script: assertion failed %s\n", qPrintable(st)); app.exit(4); return; }
                QTimer::singleShot(0, &app, *run);
            }
            else if (st.startsWith("checktext=")) {   // checktext=PIXELS: no text that is showing is smaller than this on the screen, after the interface scale (easy mode: nothing is hard to read)
                const double minPx = st.mid(10).toDouble();
                QStringList small;
                std::function<void(QQuickItem *)> walk = [&](QQuickItem *item) {
                    if (!item->isVisible() || item->opacity() <= 0.01) return;
                    const QString text = item->property("text").toString();
                    const QVariant font = item->property("font");
                    if (!text.isEmpty() && font.canConvert<QFont>()) {
                        const QFont f = font.value<QFont>();
                        const QPointF a = item->mapToScene(QPointF(0, 0)), b = item->mapToScene(QPointF(0, 1));
                        const double effective = f.pixelSize() * std::hypot(b.x() - a.x(), b.y() - a.y());
                        if (f.pixelSize() > 0 && effective < minPx - 0.01) small.append(QString("%1px: %2").arg(effective, 0, 'f', 1).arg(text.left(36)));
                    }
                    for (auto *child : item->childItems()) walk(child);
                };
                walk(window->contentItem());
                if (!small.isEmpty()) { std::fprintf(stderr, "text too small (under %g px): %s\n", minPx, qPrintable(small.mid(0, 12).join(" | "))); app.exit(7); return; }
                QTimer::singleShot(0, &app, *run);
            }
            else if (st == "checklayout") {
                if (!qmlWarnings.isEmpty()) { app.exit(6); return; }
                QStringList problems;
                // A row of a list that scrolls in place sits outside the list's box until it is scrolled to: it is clipped away, so it is not on the screen at all (only what
                // a clipping parent really shows is checked against the screen).
                std::function<void(QQuickItem *, QRectF, QRectF)> inspect = [&](QQuickItem *item, QRectF area, QRectF shown) {
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
                        if (box.intersects(shown) && (box.left() < area.left() - 1 || box.top() < area.top() - 1 || box.right() > area.right() + 1 || box.bottom() > area.bottom() + 1))
                            problems.append(text.isEmpty() ? item->objectName() : text.left(100));
                    }
                    if (item->clip()) shown = shown.intersected(item->mapRectToItem(window->contentItem(), QRectF(0, 0, item->width(), item->height())));
                    for (auto *child : item->childItems()) inspect(child, area, shown);
                };
                inspect(window->contentItem(), QRectF(0, 0, window->width(), window->height()), QRectF(0, 0, window->width(), window->height()));
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
            else if (st.startsWith("blackpicture=") && dynamic_cast<ScriptBackend *>(backend.get())) { static_cast<ScriptBackend *>(backend.get())->showBlack(st.mid(13) == "1"); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("chatmsg=")) {   // chatmsg=name|words: a chat feed appears (the demo kind) and says something
                const QStringList p = st.mid(8).split('|');
                chat.setConfigured(true, true); chat.setStatus(true, QString());
                chat.add(QString::number(1000 + chat.count()), p.value(0), p.value(1));
                QTimer::singleShot(0, &app, *run);
            }
            else if (st == "chatoff") { chat.setConfigured(false); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("setupjs=")) {   // test hook: run the script in this file on the guide's sign-in page; the answer lands in the log as "setupjs result"
                QFile f(st.mid(8));
                auto *page = window->findChild<QObject *>("setupChat");
                if (f.open(QIODevice::ReadOnly) && page) QMetaObject::invokeMethod(page, "runJs", Q_ARG(QVariant, QString::fromUtf8(f.readAll())));
                else std::fprintf(stderr, "script: setupjs needs the sign-in page and a readable file\n");
                QTimer::singleShot(0, &app, *run);
            }
            else if (st.startsWith("sourcename=")) { prefs.setSourceName(st.mid(11)); QTimer::singleShot(0, &app, *run); }   // the name of the source on X, as typed on the key page
            else if (st.startsWith("xurl=")) { prefs.noteXUrl(st.mid(5)); QTimer::singleShot(0, &app, *run); }   // the sign-in browser showed this page
            else if (st.startsWith("viewers=")) { chat.noteViewers(st.mid(8).toInt(), -1); QTimer::singleShot(0, &app, *run); }   // a reading of how many are watching, as the page would give it
            else if (st.startsWith("viewerhistory=")) {   // viewerhistory=2,2,3,4: readings 30 seconds apart, the last one now (the trend needs a minute of them, and a test cannot wait)
                const QStringList counts = st.mid(14).split('/');
                qint64 at = QDateTime::currentMSecsSinceEpoch() - 30000 * (counts.size() - 1);
                for (const QString &n : counts) { chat.noteViewersAt(n.toInt(), -1, at); at += 30000; }
                QTimer::singleShot(0, &app, *run);
            }
            else if (st == "kill") { pip.kill(); QTimer::singleShot(0, &app, *run); }   // the kill switch, as the button in the webcam window does it
            else if (st.startsWith("xattach=")) { std::printf("script: xattach %d\n", int(chat.attachX(st.mid(8)))); QTimer::singleShot(0, &app, *run); }   // a Live Studio link was copied
            else if (st.startsWith("livetitle=")) { prefs.setLiveTitle(st.mid(10)); QTimer::singleShot(0, &app, *run); }   // what the person typed as the title of their livestream
            else if (st.startsWith("livepublic=")) { prefs.setLivePublic(st.mid(11) == "1"); QTimer::singleShot(0, &app, *run); }
            else if (st.startsWith("setupstep=")) { prefs.setSetupStep(st.mid(10).toInt()); QTimer::singleShot(0, &app, *run); }   // jump the guided setup to a page
            else if (st.startsWith("clip=")) {   // text on the clipboard of THIS run only: never on a real desktop's, so only the offscreen platform may do it
                if (QGuiApplication::platformName() != "offscreen") { std::fprintf(stderr, "script: clip= works only on the offscreen platform\n"); app.exit(4); return; }
                QGuiApplication::clipboard()->setText(st.mid(5));
                QTimer::singleShot(0, &app, *run);
            }
            else if (st.startsWith("grab=")) { std::printf("script: grab %d\n", int(window->grabWindow().save(st.mid(5)))); QTimer::singleShot(0, &app, *run); }   // the window's own pixels, for the docs
            else if (st.startsWith("relaypost=")) {   // relaypost=name|words: what the other computer's script does, over a real socket to MOBS's own chat relay
                static int seq = 0;
                const QStringList p = st.mid(10).split('|');
                if (!relay.listening()) { std::fprintf(stderr, "script: the chat relay is not listening\n"); app.exit(4); return; }
                const QByteArray body = QJsonDocument(QJsonObject{{"messages", QJsonArray{QJsonObject{{"id", QString::number(++seq)}, {"name", p.value(0)}, {"text", p.value(1)}}}}}).toJson(QJsonDocument::Compact);
                auto *socket = new QTcpSocket(&app);
                QObject::connect(socket, &QTcpSocket::connected, socket, [socket, body] { socket->write("POST /chat HTTP/1.1\r\nHost: mobs\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body); });
                QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                socket->connectToHost(QHostAddress::LocalHost, relay.port());
                QTimer::singleShot(0, &app, *run);
            }
            else if (st.startsWith("piphover=") || st.startsWith("pipclick=")) {   // the pointer over (or a click on) the last showing item of that name in the webcam window
                QQuickItem *hit = pipItem(st.mid(9));
                if (!hit) { std::fprintf(stderr, "script: nothing called %s is showing in the webcam window\n", qPrintable(st.mid(9))); app.exit(4); return; }
                QQuickWindow *pw = hit->window();
                const QPointF at = hit->mapToScene(QPointF(hit->width() / 2, hit->height() / 2));
                const auto send = [&](QEvent::Type type, Qt::MouseButtons buttons) {
                    QMouseEvent e(type, at, pw->mapToGlobal(at), type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, buttons, Qt::NoModifier);
                    QCoreApplication::sendEvent(pw, &e);
                };
                send(QEvent::MouseMove, Qt::NoButton);
                if (st.startsWith("pipclick=")) { send(QEvent::MouseButtonPress, Qt::LeftButton); send(QEvent::MouseButtonRelease, Qt::NoButton); }
                QTimer::singleShot(0, &app, *run);
            }
            else if (st.startsWith("pipprobe=")) {   // what the pointer sees of a named item in the webcam window (for finding out why a hover does not show)
                QQuickItem *hit = pipItem(st.mid(9));
                if (!hit) std::fprintf(stderr, "probe: no %s showing\n", qPrintable(st.mid(9)));
                else {
                    const QPointF c = hit->mapToScene(QPointF(hit->width() / 2, hit->height() / 2));
                    std::fprintf(stderr, "probe: %s %gx%g centre at %g,%g in a %dx%d window, opacity %g\n", qPrintable(st.mid(9)), hit->width(), hit->height(), c.x(), c.y(), hit->window()->width(), hit->window()->height(), hit->opacity());
                    for (QObject *o : hit->children()) if (QByteArray(o->metaObject()->className()).contains("Handler"))
                        std::fprintf(stderr, "probe:   %s hovered=%d enabled=%d\n", o->metaObject()->className(), int(o->property("hovered").toBool()), int(o->property("enabled").toBool()));
                }
                QTimer::singleShot(0, &app, *run);
            }
            else if (st == "checkpip") {   // the dock's own layout: everything inside the window, and the cards stacked without overlapping
                QQuickWindow *pw = nullptr;
                for (QWindow *w : QGuiApplication::allWindows()) if (w->title() == PipController::kTitle && w->isVisible()) pw = qobject_cast<QQuickWindow *>(w);
                if (!pw) { std::fprintf(stderr, "script: no webcam window to check\n"); app.exit(4); return; }
                QStringList problems; QList<QRectF> cards;
                std::function<void(QQuickItem *)> inspect = [&](QQuickItem *item) {
                    if (!item->isVisible()) return;
                    const QString name = item->objectName(), text = item->property("text").toString();
                    const QRectF box = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
                    if (name == "pipVideoCard" || name == "chatPanel") cards.append(box);
                    if ((!text.isEmpty() || !name.isEmpty()) && (box.left() < -1 || box.top() < -1 || box.right() > pw->width() + 1 || box.bottom() > pw->height() + 1))
                        problems.append(name.isEmpty() ? text.left(60) : name);
                    if (name == "chatList") return;   // older messages sit scrolled out of sight inside it, on purpose
                    for (auto *child : item->childItems()) inspect(child);
                };
                inspect(pw->contentItem());
                std::sort(cards.begin(), cards.end(), [](const QRectF &a, const QRectF &b) { return a.top() < b.top(); });
                for (int i = 1; i < cards.size(); ++i) if (cards[i].top() < cards[i - 1].bottom() - 0.5) problems.append("cards overlap");
                if (!problems.isEmpty()) { std::fprintf(stderr, "dock layout (%dx%d): %s\n", pw->width(), pw->height(), qPrintable(problems.join(" | "))); app.exit(5); return; }
                QTimer::singleShot(0, &app, *run);
            }
            else if (st.startsWith("pipsize=")) {   // the webcam window dragged to another size (WxH)
                const QStringList wh = st.mid(8).split('x');
                for (auto *w : QGuiApplication::allWindows()) if (w->title() == PipController::kTitle) w->resize(wh.value(0).toInt(), wh.value(1).toInt());
                QTimer::singleShot(0, &app, *run);
            }
            else if (st == "closepip") {   // the compositor closes the webcam window, as a stray SUPER + Q does
                for (auto *w : QGuiApplication::allWindows()) if (w->title() == PipController::kTitle) w->close();
                QTimer::singleShot(0, &app, *run);
            }
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
    const int code = app.exec();
    MobsLog::write("start", QString("exit %1").arg(code));
    return code;
}
