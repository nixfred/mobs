#include "obsbackend.h"
#include "logfilter.h"
#include "mobslog.h"
#include "placement.h"
#include <obs.h>
#include <obs-nix-platform.h>
#include <obs-audio-controls.h>
#include <chrono>
#include <cmath>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QtGui/qguiapplication_platform.h>
#include <QDebug>
#include <linux/videodev2.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

namespace {
qint64 audioNow();
// Engine plugins only. obs_load_all_modules() would also load OBS's UI plugins (websocket, frontend tools...),
// which call a frontend that MOBS does not have (spike 4: crash).
const char *kModules[] = {"obs-x264", "obs-ffmpeg", "obs-outputs", "obs-filters", "obs-transitions",
                          "image-source", "text-freetype2", "linux-pipewire", "linux-pulseaudio",
                          "linux-v4l2", "linux-capture", "obs-nvenc", "obs-qsv11"};

QString readToken(const QString &file) {
    QFile f(file);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readLine()).trimmed() : QString();
}
void writeToken(const QString &file, const QString &token) {
    QFile f(file);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) { f.write(token.toUtf8()); f.write("\n"); f.setPermissions(QFile::ReadOwner | QFile::WriteOwner); }
}

// The broadcast target: a server address and a key. OBS's own "custom server" service lives in its rtmp-services module, which
// also asks obsproject.com for a service list every time it loads. MOBS never phones home, so it registers this tiny one instead.
struct StreamTarget { std::string url, key; };
const char *targetName(void *) { return "MOBS stream target"; }
void *targetCreate(obs_data_t *settings, obs_service_t *) {
    auto *t = new StreamTarget;
    t->url = obs_data_get_string(settings, "server");
    t->key = obs_data_get_string(settings, "key");
    return t;
}
void targetDestroy(void *data) { delete static_cast<StreamTarget *>(data); }
const char *targetUrl(void *data) { return static_cast<StreamTarget *>(data)->url.c_str(); }
const char *targetKey(void *data) { return static_cast<StreamTarget *>(data)->key.c_str(); }
const char *targetProtocol(void *data) { return std::strncmp(targetUrl(data), "rtmps://", 8) == 0 ? "RTMPS" : "RTMP"; }
const char *targetInfo(void *data, uint32_t type) {
    switch (type) {
    case OBS_SERVICE_CONNECT_INFO_SERVER_URL: return targetUrl(data);
    case OBS_SERVICE_CONNECT_INFO_STREAM_ID: return targetKey(data);
    default: return nullptr;
    }
}
bool targetCanConnect(void *data) { return targetUrl(data)[0] != '\0'; }
void registerStreamTarget() {
    static obs_service_info info{};
    info.id = "mobs_rtmp";
    info.get_name = targetName; info.create = targetCreate; info.destroy = targetDestroy;
    info.get_url = targetUrl; info.get_key = targetKey; info.get_protocol = targetProtocol;
    info.get_connect_info = targetInfo; info.can_try_to_connect = targetCanConnect;
    obs_register_service(&info);
}
}

static_assert(LOG_ERROR == obslog::Error && LOG_WARNING == obslog::Warning && LOG_INFO == obslog::Info && LOG_DEBUG == obslog::Debug,
              "libobs changed its log levels: update logfilter.h");

static void obsLogHandler(int level, const char *fmt, va_list args, void *) {
    static const bool verbose = qEnvironmentVariable("MOBS_VERBOSE") == "1";
    const bool toFile = level <= obslog::Info;         // the log FILE keeps libobs's informational lines (never its per-frame debug ones)
    const bool toJournal = obsLogWanted(level, verbose);   // the journal still only gets warnings and errors
    if (!toFile && !toJournal) return;                 // (a debug line per camera frame is dropped before anything is formatted)
    const QString line = QString::vasprintf(fmt, args);
    if (toFile) MobsLog::write(level <= obslog::Error ? "libobs-error" : level <= obslog::Warning ? "libobs-warn" : "libobs", line);
    // The file log scrubs the stream key in write(); the copy on stderr goes to the journal under a launcher, so it is scrubbed the same way.
    if (toJournal) fprintf(stderr, "%s\n", MobsLog::scrub(line).toLocal8Bit().constData());
}

ObsBackend::ObsBackend(QString tokenFile, QObject *parent) : RecorderBackend(parent), m_tokenFile(std::move(tokenFile)) {
    m_graphTick.setInterval(300);
    connect(&m_graphTick, &QTimer::timeout, this, &ObsBackend::tickGraph);
    m_poll.setInterval(100);
    connect(&m_poll, &QTimer::timeout, this, &ObsBackend::poll);
    m_audioTick.setInterval(100);
    connect(&m_audioTick, &QTimer::timeout, this, &ObsBackend::pollAudio);
}

ObsBackend::~ObsBackend() {
    m_poll.stop();
    m_previewOn = false;
    removePreviewCallback();
    stopOutput();
    dropGraph();
    if (m_libobsUp) {
        obs_wait_for_destroy_queue();   // releases are queued to libobs's graphics thread; skipping this crashes
        obs_shutdown();
    }
}

bool ObsBackend::ensureStarted(QString *error) {
    if (m_libobsUp) return true;
    // The platform and display MUST be set before obs_startup (OBS's own OBSApp::OBSInit does the same).
    if (QGuiApplication::platformName().contains("wayland")) {
        auto native = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
        if (!native) { *error = "no Wayland display to draw on"; return false; }
        obs_set_nix_platform_display(native->display());
        obs_set_nix_platform(OBS_NIX_PLATFORM_WAYLAND);
    } else {
        *error = "MOBS records on Wayland only (platform: " + QGuiApplication::platformName() + ")";
        return false;
    }
    base_set_log_handler(obsLogHandler, nullptr);   // before obs_startup, so even its first lines are filtered
    if (!obs_startup("en-US", nullptr, nullptr)) { *error = "the recording engine would not start"; return false; }
    registerStreamTarget();
    obs_add_data_path("/usr/share/obs/libobs/");
    obs_add_module_path("/usr/lib/obs-plugins/", "/usr/share/obs/obs-plugins/%module%");

    obs_video_info ovi{};
    ovi.graphics_module = "libobs-opengl";
    ovi.fps_num = 30; ovi.fps_den = 1;
    ovi.base_width = 1920; ovi.base_height = 1080;
    ovi.output_width = 1920; ovi.output_height = 1080;
    ovi.output_format = VIDEO_FORMAT_NV12;
    ovi.colorspace = VIDEO_CS_709; ovi.range = VIDEO_RANGE_PARTIAL;
    ovi.scale_type = OBS_SCALE_BICUBIC;
    ovi.gpu_conversion = true;
    if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS) { *error = "the video engine would not start (OpenGL)"; obs_shutdown(); return false; }
    obs_audio_info2 oai{};
    oai.samples_per_sec = 44100; oai.speakers = SPEAKERS_STEREO;   // 44.1 kHz: what X recommends for a stream (the file uses it too)
    if (!obs_reset_audio2(&oai)) { *error = "the audio engine would not start"; obs_shutdown(); return false; }

    for (const char *m : kModules) {
        obs_module_t *mod = nullptr;
        const std::string bin = std::string("/usr/lib/obs-plugins/") + m + ".so";
        const std::string data = std::string("/usr/share/obs/obs-plugins/") + m;
        if (obs_open_module(&mod, bin.c_str(), data.c_str()) == MODULE_SUCCESS) obs_init_module(mod);   // nvenc may refuse: fine
    }
    obs_post_load_modules();
    m_libobsUp = true;
    return true;
}

static bool encoderMissing(obs_encoder_t *encoder) { return !encoder || !obs_encoder_get_codec(encoder); }

static void fillCanvas(obs_scene_item *item) {
    vec2 bounds{float(Placement::kCanvasW), float(Placement::kCanvasH)};
    obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_INNER);
    obs_sceneitem_set_bounds(item, &bounds);
}

// ---------------------------------------------------------------- the graph (scene and sources)

void ObsBackend::applyPlacement(const RecordSpec &spec) {
    if (!m_camItem || spec.pick != 2) return;   // only "screen + me" has a placement
    const QRect r = spec.camera.x >= 0 ? Placement::cameraRectAt(spec.camera.x, spec.camera.y, spec.camera.size) : Placement::cameraRect(spec.camera.corner, spec.camera.size);
    vec2 pos{float(r.x()), float(r.y())}, bounds{float(r.width()), float(r.height())};
    obs_sceneitem_set_pos(m_camItem, &pos);
    obs_sceneitem_set_bounds_type(m_camItem, OBS_BOUNDS_SCALE_INNER);
    obs_sceneitem_set_bounds(m_camItem, &bounds);
}

bool ObsBackend::ensureGraph(const RecordSpec &spec, QString *error) {
    if (m_graphUp && spec.pick == m_graphPick && spec.camera.path == m_graphCamera && spec.camera.pip == m_graphPip) {
        applyPlacement(spec);                          // moving the webcam needs no new scene (nor a new picker), so it may happen mid-take or mid-broadcast
        return true;
    }
    if (m_graphUp) {
        if (m_phase != Off) return true;               // recording: the scene must not change under it
        dropGraph();
    }
    m_scene = obs_scene_create("mobs");
    m_needScreen = spec.pick != 3;
    m_needCamera = spec.pick != 1 && !(spec.pick == 2 && spec.camera.pip);   // in pip mode the webcam is a window on the screen, not part of this picture
    m_graphPip = spec.camera.pip;
    m_testScreen = qEnvironmentVariable("MOBS_TEST_SOURCE") == "colour";    // tests run without the picker
    const QString testCam = qEnvironmentVariable("MOBS_TEST_CAMERA");       // "colour" or "video:/path/file.mp4": no webcam needed
    m_testCamera = testCam == "colour" || testCam.startsWith("video:");

    if (m_needScreen) {
        if (m_testScreen) {
            obs_data_t *cs = obs_data_create();
            obs_data_set_int(cs, "color", qEnvironmentVariable("MOBS_TEST_COLOUR", "FF3050F0").toUInt(nullptr, 16));   // ABGR
            obs_data_set_int(cs, "width", 1920); obs_data_set_int(cs, "height", 1080);
            m_video = obs_source_create("color_source_v3", "test colour", cs, nullptr);
            obs_data_release(cs);
        } else {
            // Created here, on the Qt main thread: the portal's reply is handled where the source was made (spike 3b/4b).
            obs_data_t *ps = obs_data_create();
            obs_data_set_bool(ps, "ShowCursor", true);
            const QString token = readToken(screenTokenFile());
            if (!token.isEmpty()) obs_data_set_string(ps, "RestoreToken", token.toUtf8().constData());
            // "My screen" and "screen + me" share a whole SCREEN, never a window or a dragged region: that is what lets you change
            // workspaces while sharing, and a window or region is what made half a picture and a black rest.
            m_video = obs_source_create("pipewire-desktop-capture-source", "my screen", ps, nullptr);
            if (!m_video) m_video = obs_source_create("pipewire-screen-capture-source", "my screen", ps, nullptr);   // an OBS without the screen-only source
            obs_data_release(ps);
            MobsLog::write("capture", QString("screen source created (whole-screen capture), %1").arg(token.isEmpty() ? "no saved choice: the picker opens" : "restoring the saved choice"));
            if (token.isEmpty()) emit waitingForUser("choose the screen to share in the dialog that just opened: the whole screen, so you can change workspaces");
        }
        if (!m_video) { *error = "could not create the screen source (is the PipeWire plugin installed?)"; dropGraph(); return false; }
        m_item = obs_scene_add(m_scene, m_video);   // first in the scene = at the back
        fillCanvas(m_item);
    }

    if (m_needCamera) {
        if (m_testCamera && testCam.startsWith("video:")) {
            // An ASYNC source like a real webcam: its size is unknown until its first frame arrives. A solid colour source
            // has its size from the start and hides the bug this exists to catch.
            obs_data_t *cs = obs_data_create();
            obs_data_set_bool(cs, "is_local_file", true);
            obs_data_set_string(cs, "local_file", testCam.mid(6).toUtf8().constData());
            obs_data_set_bool(cs, "looping", true);
            m_camera = obs_source_create("ffmpeg_source", "test camera video", cs, nullptr);
            obs_data_release(cs);
        } else if (m_testCamera) {
            obs_data_t *cs = obs_data_create();
            obs_data_set_int(cs, "color", qEnvironmentVariable("MOBS_TEST_CAMERA_COLOUR", "FF00C800").toUInt(nullptr, 16));
            obs_data_set_int(cs, "width", 1280); obs_data_set_int(cs, "height", 720);
            m_camera = obs_source_create("color_source_v3", "test camera", cs, nullptr);
            obs_data_release(cs);
        } else {
            obs_data_t *cs = obs_data_create();
            obs_data_set_string(cs, "device_id", spec.camera.path.toUtf8().constData());
            if (spec.camera.mjpg720) {                                            // otherwise the camera's own (often slow, small) default
                obs_data_set_int(cs, "pixelformat", V4L2_PIX_FMT_MJPEG);
                obs_data_set_int(cs, "resolution", (qint64(1280) << 32) | 720);   // the v4l2 plugin packs width and height like this
                // Webcam MJPEG is FULL range (0-255). Left at "default" libobs treats it as limited (16-235), which crushes
                // dark pixels to black: in a dim room most of the picture disappears.
                obs_data_set_int(cs, "color_range", VIDEO_RANGE_FULL);
            }
            m_camera = obs_source_create("v4l2_input", "camera", cs, nullptr);
            obs_data_release(cs);
        }
        if (!m_camera) { *error = "could not create the camera source (is the v4l2 plugin installed?)"; dropGraph(); return false; }
        m_camItem = obs_scene_add(m_scene, m_camera);   // added second = drawn on top of the screen
        if (spec.pick == 3) fillCanvas(m_camItem);      // just my camera: it fills the frame
        else applyPlacement(spec);
    }
    obs_set_output_source(0, obs_scene_get_source(m_scene));
    m_graphUp = true;
    m_graphPick = spec.pick;
    m_graphCamera = spec.camera.path;
    MobsLog::write("capture", QString("scene built: pick=%1 camera=%2").arg(spec.pick).arg(spec.camera.valid() ? spec.camera.path : "none"));
    m_loggedScreen = m_loggedCamera = false;
    m_graphTick.start();   // the sizes of what is captured are logged once known
    tickGraph();
    return true;
}

void ObsBackend::tickGraph() {
    if (!m_graphUp) return;
    // A source's size is only known after its first frame. Logged once each: what MOBS really captured is the first thing to look at when a picture is wrong.
    if (m_video && !m_loggedScreen && obs_source_get_width(m_video) > 0) {
        m_loggedScreen = true;
        emit captureSized(int(obs_source_get_width(m_video)), int(obs_source_get_height(m_video)));
        MobsLog::write("capture", QString("screen source is %1x%2 (%3)").arg(obs_source_get_width(m_video)).arg(obs_source_get_height(m_video)).arg(m_testScreen ? "test colour" : "whole screen"));
        saveScreenChoice();   // the choice is remembered as soon as it is made, by the guide's preview as much as by a take: the person is asked once, not once per source
    }
    if (m_camera && !m_loggedCamera && obs_source_get_width(m_camera) > 0) {
        m_loggedCamera = true;
        MobsLog::write("capture", QString("camera source is %1x%2").arg(obs_source_get_width(m_camera)).arg(obs_source_get_height(m_camera)));
    }
}

// Save the restore token of the screen share, once the picker has been answered (the first picture is in). On 2026-10-13 the picker opened twice before a person could go live: the
// guide's preview made its choice, the guide ended and the preview was rebuilt, and nothing had kept the first choice, because only a take saved it.
void ObsBackend::saveScreenChoice() {
    if (!m_needScreen || m_testScreen || !m_video) return;
    obs_source_save(m_video);
    obs_data_t *st = obs_source_get_settings(m_video);
    const QString tok = QString::fromUtf8(obs_data_get_string(st, "RestoreToken"));
    obs_data_release(st);
    if (!tok.isEmpty() && tok != readToken(screenTokenFile())) writeToken(screenTokenFile(), tok);
}

// The whole-screen choice is remembered in its own file, apart from the plain token file that an older MOBS kept for a window choice:
// a token for one kind of choice must not be fed to the other.
QString ObsBackend::screenTokenFile() const { return m_tokenFile + "-desktop"; }

void ObsBackend::forgetCapture() {
    QFile::remove(m_tokenFile); QFile::remove(screenTokenFile());   // the plain file is the older window choice, which nothing reads any more
    if (m_phase != Off) return;                     // a take keeps the picture it has; the next one asks
    const bool wasPreviewing = m_previewOn;
    if (m_graphUp) dropGraph();
    if (wasPreviewing) {                            // the picture is on: rebuild it now, which opens the dialog
        QString error;
        if (!ensureStarted(&error) || !ensureGraph(m_previewSpec, &error)) { m_previewOn = false; removePreviewCallback(); emit previewFailed(error); }
    }
}

void ObsBackend::dropGraph() {
    // Order matters (spike 4): detach from the output, remove scene items, release sources, then the scene.
    if (!m_graphUp && !m_scene && !m_video && !m_camera) return;
    m_graphTick.stop();
    if (m_libobsUp) obs_set_output_source(0, nullptr);
    if (m_item) { obs_sceneitem_remove(m_item); m_item = nullptr; }
    if (m_camItem) { obs_sceneitem_remove(m_camItem); m_camItem = nullptr; }
    if (m_camera) { obs_source_release(m_camera); m_camera = nullptr; }
    if (m_video) { obs_source_release(m_video); m_video = nullptr; }
    if (m_scene) { obs_scene_release(m_scene); m_scene = nullptr; }
    if (m_libobsUp) obs_wait_for_destroy_queue();
    m_graphUp = false;
    m_graphPick = 0;
    m_graphCamera.clear();
    m_graphPip = false;
}

bool ObsBackend::sourcesReady() const {
    const bool screenOk = !m_needScreen || m_testScreen || obs_source_get_width(m_video) > 0;
    const bool cameraOk = !m_needCamera || m_testCamera || obs_source_get_width(m_camera) > 0;
    return screenOk && cameraOk;
}

// ---------------------------------------------------------------- the recording (mic, encoders, file)

bool ObsBackend::startOutput(const RecordSpec &spec, QString *error) {
    m_audioStarted = audioNow();
    // The AAC track always exists; with both inputs off it contains silence.
    if (spec.mic && !startMic(spec.audio, error)) return false;
    if (spec.audio.desktop) {
        obs_data_t *settings = obs_data_create();
        const QString test = qEnvironmentVariable("MOBS_TEST_DESKTOP_AUDIO");
        const bool fake = test.startsWith("file:");
        obs_data_set_string(settings, "device_id", spec.audio.desktopDevice.toUtf8().constData());
        if (fake) {
            obs_data_set_bool(settings, "is_local_file", true); obs_data_set_bool(settings, "looping", true);
            obs_data_set_string(settings, "local_file", test.mid(5).toUtf8().constData());
        }
        m_desktop = obs_source_create(fake ? "ffmpeg_source" : "pulse_output_capture", "desktop audio", settings, nullptr);
        obs_data_release(settings);
        if (!m_desktop) { *error = "could not create desktop audio capture"; return false; }
        obs_source_set_audio_mixers(m_desktop, 1);
        obs_source_set_volume(m_desktop, obs_db_to_mul(spec.audio.desktopGainDb));
        obs_set_output_source(2, m_desktop);
        m_desktopDb = -60; m_desktopFrameAt = 0;
        m_desktopMeter = obs_volmeter_create(OBS_FADER_LOG);
        obs_volmeter_add_callback(m_desktopMeter, &ObsBackend::desktopLevel, this);
        obs_volmeter_attach_source(m_desktopMeter, m_desktop);
        m_audioTick.start();
    }

    obs_data_t *vs = obs_data_create();
    if (spec.hardware) {
        obs_data_set_string(vs, "rate_control", "CQP"); obs_data_set_int(vs, "cqp", spec.crf);
        obs_data_set_string(vs, "preset", "p5");
    } else {
        obs_data_set_string(vs, "preset", spec.encoderPreset.toUtf8().constData());
        obs_data_set_string(vs, "rate_control", "CRF"); obs_data_set_int(vs, "crf", spec.crf);
    }
    m_venc = obs_video_encoder_create(spec.hardware ? "obs_nvenc_h264_tex" : "obs_x264", "mobs video", vs, nullptr);
    obs_data_release(vs);
    obs_data_t *as = obs_data_create();
    obs_data_set_int(as, "bitrate", 160);
    m_aenc = obs_audio_encoder_create("ffmpeg_aac", "mobs audio", as, 0, nullptr);
    obs_data_release(as);
    // libobs hands back a placeholder (no codec) for an encoder whose plugin did not load, so a null check alone misses a missing NVIDIA driver.
    if (encoderMissing(m_venc) || encoderMissing(m_aenc)) { *error = spec.hardware ? "NVIDIA H.264 encoding is unavailable; choose software in EXPERT > hardware" : "no video or audio encoder is available"; return false; }
    obs_encoder_set_video(m_venc, obs_get_video());
    obs_encoder_set_audio(m_aenc, obs_get_audio());

    obs_data_t *os = obs_data_create();
    obs_data_set_string(os, "path", spec.path.toUtf8().constData());
    m_out = obs_output_create("mp4_output", "mobs recording", os, nullptr);
    obs_data_release(os);
    if (!m_out) { *error = "could not create the file output"; return false; }
    m_stopCode = 1;
    signal_handler_connect(obs_output_get_signal_handler(m_out), "stop", &ObsBackend::outputStopped, this);
    obs_output_set_video_encoder(m_out, m_venc);
    obs_output_set_audio_encoder(m_out, m_aenc, 0);
    // The picker can stay open long after the name was chosen. Claim the path atomically
    // only when ready to record, so another take cannot be truncated during that delay.
    QFile reservation(spec.path);
    if (!reservation.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        const QFileInfo existing(spec.path);
        *error = existing.exists() || existing.isSymLink()
            ? "that recording filename already exists; start a new take to use another name"
            : "cannot create the recording file: " + reservation.errorString();
        return false;
    }
    reservation.close();
    // The broadcast goes first: if it cannot even start, nothing was recorded and the user is told, instead of a take that
    // quietly never went live. The file is the safety net and is started right behind it.
    if (spec.stream.on && !startStream(spec, error)) {
        if (QFileInfo(spec.path).size() == 0) QFile::remove(spec.path);
        return false;
    }
    if (!obs_output_start(m_out)) {
        const QString why = QString::fromUtf8(obs_output_get_last_error(m_out));
        *error = why.isEmpty() ? "the recording would not start" : why;
        if (spec.hardware) *error += "; try software in EXPERT > hardware";
        if (QFileInfo(spec.path).size() == 0) QFile::remove(spec.path);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------- the broadcast (its own encoders and RTMP output)

QString ObsBackend::redacted(QString text) const {
    if (!m_streamKey.isEmpty()) text.replace(m_streamKey, "***");
    return text;
}

void ObsBackend::streamStarted(void *self, calldata *) { static_cast<ObsBackend *>(self)->m_streamUp = true; }
void ObsBackend::streamReconnecting(void *self, calldata *) { static_cast<ObsBackend *>(self)->m_streamRetrying = true; }
void ObsBackend::streamReconnected(void *self, calldata *) { static_cast<ObsBackend *>(self)->m_streamRetrying = false; }
void ObsBackend::streamStopped(void *self, calldata *data) { static_cast<ObsBackend *>(self)->m_streamStop = int(calldata_int(data, "code")); }

bool ObsBackend::startStream(const RecordSpec &spec, QString *error) {
    const StreamSpec &s = spec.stream;
    m_streamKey = s.key;
    MobsLog::secret(s.key);                                              // from here on the key can never reach the log, whatever prints it
    MobsLog::write("stream", QString("starting: server %1, %2 kbps, %3p, %4").arg(s.url).arg(s.kbps).arg(s.height).arg(spec.hardware ? "NVIDIA" : "x264"));
    m_streamStop = 1; m_streamUp = false; m_streamRetrying = false; m_streamAsked = false; m_streamShown.clear();
    m_health.reset(); m_streamAdvice.clear(); m_streamGaveUp = false; m_streamTime.start();

    obs_data_t *target = obs_data_create();
    obs_data_set_string(target, "server", s.url.toUtf8().constData());
    obs_data_set_string(target, "key", s.key.toUtf8().constData());
    m_service = obs_service_create("mobs_rtmp", "mobs stream target", target, nullptr);
    obs_data_release(target);
    if (!m_service) { *error = "could not set up the stream target"; return false; }

    // A server wants a steady bitrate and a keyframe every couple of seconds; the file's quality-based encoder gives neither.
    obs_data_t *vs = obs_data_create();
    obs_data_set_string(vs, "rate_control", "CBR");
    obs_data_set_int(vs, "bitrate", s.kbps);
    obs_data_set_int(vs, "keyint_sec", 3);   // X recommends a keyframe every 3 seconds
    if (spec.hardware) {
        obs_data_set_string(vs, "preset", "p5");
        obs_data_set_int(vs, "bf", 0); obs_data_set_bool(vs, "lookahead", false);   // live ingest: no B-frames and no look-ahead delay
    } else {
        obs_data_set_string(vs, "x264opts", "bframes=0");
        obs_data_set_string(vs, "preset", "veryfast");   // live cannot wait for a slow preset, whatever the file uses
        obs_data_set_string(vs, "profile", "high");
        obs_data_set_bool(vs, "use_bufsize", true); obs_data_set_int(vs, "buffer_size", s.kbps);
    }
    m_svenc = obs_video_encoder_create(spec.hardware ? "obs_nvenc_h264_tex" : "obs_x264", "mobs stream video", vs, nullptr);
    obs_data_release(vs);
    obs_data_t *as = obs_data_create();
    obs_data_set_int(as, "bitrate", 128);
    m_saenc = obs_audio_encoder_create("ffmpeg_aac", "mobs stream audio", as, 0, nullptr);
    obs_data_release(as);
    if (encoderMissing(m_svenc) || encoderMissing(m_saenc)) { *error = spec.hardware ? "NVIDIA H.264 is unavailable for the stream; choose software in EXPERT > hardware" : "no video or audio encoder is available for the stream"; return false; }
    obs_encoder_set_video(m_svenc, obs_get_video());
    if (s.height == 720) obs_encoder_set_scaled_size(m_svenc, 1280, 720);
    obs_encoder_set_audio(m_saenc, obs_get_audio());

    obs_data_t *os = obs_data_create();
    obs_data_set_int(os, "max_shutdown_time_sec", 5);    // a dead network must not hold the saved file hostage
    obs_data_set_bool(os, "dyn_bitrate", true);          // on a congested uplink lower the bitrate instead of dropping frames (and the connection)
    m_stream = obs_output_create("rtmp_output", "mobs stream", os, nullptr);
    obs_data_release(os);
    if (!m_stream) { *error = "could not create the stream output"; return false; }
    obs_output_set_service(m_stream, m_service);
    obs_output_set_video_encoder(m_stream, m_svenc);
    obs_output_set_audio_encoder(m_stream, m_saenc, 0);
    obs_output_set_reconnect_settings(m_stream, 10, 2); // a dropped connection is retried with growing waits (2 s, 3 s, 4.5 s ... about 77 s): roughly four minutes, then it says so; the file is unaffected
    signal_handler_t *sh = obs_output_get_signal_handler(m_stream);
    signal_handler_connect(sh, "start", &ObsBackend::streamStarted, this);
    signal_handler_connect(sh, "reconnect", &ObsBackend::streamReconnecting, this);
    signal_handler_connect(sh, "reconnect_success", &ObsBackend::streamReconnected, this);
    signal_handler_connect(sh, "stop", &ObsBackend::streamStopped, this);
    if (!obs_output_start(m_stream)) {
        const QString why = redacted(QString::fromUtf8(obs_output_get_last_error(m_stream)));
        *error = why.isEmpty() ? "the stream would not start: check the server address and stream key" : "the stream would not start: " + why;
        return false;
    }
    m_streamShown = "connecting";
    m_streamClock.start(); m_streamBytes = 0;
    emit streamState("connecting", QString());
    return true;
}

void ObsBackend::stopStream() {
    if (m_stream) {
        signal_handler_t *sh = obs_output_get_signal_handler(m_stream);
        signal_handler_disconnect(sh, "start", &ObsBackend::streamStarted, this);
        signal_handler_disconnect(sh, "reconnect", &ObsBackend::streamReconnecting, this);
        signal_handler_disconnect(sh, "reconnect_success", &ObsBackend::streamReconnected, this);
        signal_handler_disconnect(sh, "stop", &ObsBackend::streamStopped, this);
        if (obs_output_active(m_stream)) obs_output_force_stop(m_stream);   // a graceful stop was already asked for by end()
        obs_output_release(m_stream); m_stream = nullptr;
    }
    if (m_svenc) { obs_encoder_release(m_svenc); m_svenc = nullptr; }
    if (m_saenc) { obs_encoder_release(m_saenc); m_saenc = nullptr; }
    if (m_service) { obs_service_release(m_service); m_service = nullptr; }
    m_streamKey.clear();
    MobsLog::secret({});
    m_spec.stream.key.clear();   // the password is forgotten as soon as the broadcast is over
}

// Runs every 100 ms on the Qt thread: turns what libobs reported from its own threads into one state, announced once. It also notices X refusing
// the stream: a connection that is accepted and then dropped within seconds, three times in a row, is not a hiccup (see StreamHealth).
void ObsBackend::pollStream() {
    if (!m_stream) return;
    QString state, detail;
    const int code = m_streamStop.load();
    if (code != 1) {
        if (m_streamAsked || code == OBS_OUTPUT_SUCCESS) state = "ended";
        else {
            state = "failed";
            switch (code) {
            case OBS_OUTPUT_BAD_PATH: detail = "the server address is not valid"; break;
            case OBS_OUTPUT_CONNECT_FAILED: detail = "could not connect to the server: check the address and your internet"; break;
            case OBS_OUTPUT_INVALID_STREAM: detail = "the server did not accept the address or stream key"; break;
            case OBS_OUTPUT_DISCONNECTED: detail = "the connection to the server was lost"; break;
            case OBS_OUTPUT_UNSUPPORTED: detail = "the server does not support this kind of stream"; break;
            case OBS_OUTPUT_ENCODE_ERROR: detail = "the stream encoder failed"; break;
            default: detail = QString("the stream stopped (code %1)").arg(code);
            }
            const QString more = redacted(QString::fromUtf8(obs_output_get_last_error(m_stream))).left(160);
            if (!more.isEmpty() && !detail.contains(more)) detail += " (" + more + ")";
        }
    } else if (m_streamRetrying) {
        state = "reconnecting"; detail = "the connection dropped: trying again";
    } else state = m_streamUp ? "live" : "connecting";
    if (m_streamGaveUp) { state = "failed"; detail = m_streamAdvice; }   // we stopped it ourselves, with a reason worth reading: nothing overrides that

    const quint64 bytes = obs_output_get_total_bytes(m_stream);
    if (state != m_streamShown) {
        const QString before = m_streamShown;
        if (state == "live") {
            m_streamBytes = bytes;                                       // a new connection starts libobs's byte count again
            m_health.connected(m_streamTime.elapsed(), bytes);
            MobsLog::write("stream", QString("connected (the server answered in %1 ms)").arg(obs_output_get_connect_time_ms(m_stream)));
        }
        if (before == "live" && (state == "reconnecting" || state == "failed")) {
            const StreamHealth::Verdict verdict = m_health.dropped(m_streamTime.elapsed(), bytes);
            MobsLog::write("stream", "dropped: " + verdict.note + (detail.isEmpty() ? QString() : " (" + detail + ")"));
            if (verdict.giveUp && !m_streamGaveUp) {
                m_streamGaveUp = true; m_streamAdvice = verdict.advice;
                state = "failed"; detail = verdict.advice;
                MobsLog::write("stream", "giving up: " + verdict.advice);
                obs_output_stop(m_stream);                            // cancels the retries; the recording carries on
            }
        }
        m_streamShown = state; emit streamState(state, detail);
    }
    if (state == "live" && m_streamClock.elapsed() >= 1000) {
        const double seconds = m_streamClock.restart() / 1000.0;
        if (bytes < m_streamBytes) m_streamBytes = 0;                    // the counter restarted since the last look
        emit streamStats(int((bytes - m_streamBytes) * 8 / 1000 / seconds), obs_output_get_frames_dropped(m_stream));
        m_streamBytes = bytes;
    }
}

namespace {
qint64 audioNow() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
float peakDb(const float *peaks) {
    float level = -60;
    for (int i = 0; i < MAX_AUDIO_CHANNELS; ++i) if (std::isfinite(peaks[i])) level = std::max(level, peaks[i]);
    return level;
}
}

void ObsBackend::micLevel(void *self, const float *, const float *peak, const float *) {
    auto *backend = static_cast<ObsBackend *>(self); backend->m_micDb = peakDb(peak); backend->m_micFrameAt = audioNow();
}
void ObsBackend::desktopLevel(void *self, const float *, const float *peak, const float *) {
    auto *backend = static_cast<ObsBackend *>(self); backend->m_desktopDb = peakDb(peak); backend->m_desktopFrameAt = audioNow();
}
bool ObsBackend::startMic(const AudioSpec &audio, QString *error) {
    obs_data_t *settings = obs_data_create();
    const QString test = qEnvironmentVariable("MOBS_TEST_AUDIO"); const bool fake = test.startsWith("file:");
    obs_data_set_string(settings, "device_id", audio.micDevice.toUtf8().constData());
    if (fake) {
        obs_data_set_bool(settings, "is_local_file", true); obs_data_set_bool(settings, "looping", true);
        obs_data_set_string(settings, "local_file", test.mid(5).toUtf8().constData());
    }
    m_mic = obs_source_create(fake ? "ffmpeg_source" : "pulse_input_capture", "mic", settings, nullptr); obs_data_release(settings);
    if (!m_mic) { *error = "could not create microphone input"; return false; }
    obs_source_set_audio_mixers(m_mic, 1); obs_source_set_volume(m_mic, obs_db_to_mul(audio.micGainDb));
    obs_set_output_source(1, m_mic);
    m_audioStarted = audioNow(); m_micFrameAt = 0; m_micDb = -60;
    m_micMeter = obs_volmeter_create(OBS_FADER_LOG);
    obs_volmeter_add_callback(m_micMeter, &ObsBackend::micLevel, this); obs_volmeter_attach_source(m_micMeter, m_mic);
    m_audioTick.start(); return true;
}
void ObsBackend::stopAudio() {
    m_audioTick.stop();
    if (m_micMeter) { obs_volmeter_remove_callback(m_micMeter, &ObsBackend::micLevel, this); obs_volmeter_destroy(m_micMeter); m_micMeter = nullptr; }
    if (m_desktopMeter) { obs_volmeter_remove_callback(m_desktopMeter, &ObsBackend::desktopLevel, this); obs_volmeter_destroy(m_desktopMeter); m_desktopMeter = nullptr; }
    if (m_libobsUp) { obs_set_output_source(1, nullptr); obs_set_output_source(2, nullptr); }
    if (m_mic) { obs_source_release(m_mic); m_mic = nullptr; }
    if (m_desktop) { obs_source_release(m_desktop); m_desktop = nullptr; }
    emit audioLevels(-60, -60, false, false, QString());
}
void ObsBackend::pollAudio() {
    const qint64 now = audioNow();
    const bool micFrames = m_mic && m_micFrameAt && now - m_micFrameAt.load() < 500;
    const bool desktopFrames = m_desktop && m_desktopFrameAt && now - m_desktopFrameAt.load() < 500;
    QString problem;
    if (now - m_audioStarted > 4000) {
        if (m_mic && !micFrames) problem = "microphone is not sending audio: press the microphone button and choose another";
        if (m_desktop && !desktopFrames) problem += (problem.isEmpty() ? "" : "\n") + QString("desktop audio is not sending frames; check the selected output");
    }
    emit audioLevels(micFrames ? m_micDb.load() : -60, desktopFrames ? m_desktopDb.load() : -60, micFrames, desktopFrames, problem);
}
void ObsBackend::setMicCheck(bool on, const AudioSpec &audio) {
    if (m_phase != Off) return; // changes during a take apply to the next take
    stopAudio();
    if (!on) return;
    QString error;
    if (!ensureStarted(&error) || !startMic(audio, &error)) { stopAudio(); emit micCheckFailed(error); }
}
void ObsBackend::checkHardware() {
    QString error;
    if (!ensureStarted(&error)) { emit hardwareChecked(false, error); return; }
    bool nvenc = false; const char *id;
    for (size_t n = 0; obs_enum_encoder_types(n, &id); ++n) if (QString::fromUtf8(id) == "obs_nvenc_h264_tex") nvenc = true;
    emit hardwareChecked(nvenc, nvenc ? "NVIDIA H.264 is available" : "NVIDIA H.264 is unavailable; software encoding works");
}

void ObsBackend::stopOutput() {
    if (m_stream) {
        // The take is over, so the broadcast is too. Say so once, unless it already said "failed" or "ended".
        if (m_streamShown != "failed" && m_streamShown != "ended") emit streamState("ended", QString());
        m_streamShown = "ended";
        stopStream();
    }
    // Also when the take failed before the broadcast got an output (the picker timed out, the camera never started, no encoder): the key is kept for one take only, and the
    // scrubber that hides it from every line is switched off with it (stopStream does the same, but only when there was an output).
    m_spec.stream.key.clear(); m_streamKey.clear(); MobsLog::secret({});
    if (m_out) {
        signal_handler_disconnect(obs_output_get_signal_handler(m_out), "stop", &ObsBackend::outputStopped, this);
        obs_output_release(m_out); m_out = nullptr;
    }
    if (m_venc) { obs_encoder_release(m_venc); m_venc = nullptr; }
    if (m_aenc) { obs_encoder_release(m_aenc); m_aenc = nullptr; }
    stopAudio();
    if (m_libobsUp) obs_wait_for_destroy_queue();
}

// An attempt (waiting or recording) is over: drop the recording parts, and the scene too unless it is still being previewed.
void ObsBackend::finishAttempt() {
    m_poll.stop();
    stopOutput();
    m_phase = Off;
    if (!m_previewOn) dropGraph();
}

void ObsBackend::fail(const QString &why) {
    finishAttempt();
    emit failed(why);
}

void ObsBackend::begin(const RecordSpec &spec) {
    if (m_phase != Off) return;
    stopAudio(); // a preflight microphone check must not survive into the take
    QString error;
    if (!ensureStarted(&error) || !ensureGraph(spec, &error)) {
        fail(error);
        return;
    }
    m_path = spec.path;
    m_spec = spec;
    m_phase = Waiting;
    m_waited.start();
    m_screenReady.invalidate();
    m_poll.start();
}

void ObsBackend::poll() {
    if (m_phase == Waiting) {
        const bool screenUp = !m_needScreen || m_testScreen || obs_source_get_width(m_video) > 0;
        if (screenUp && !m_screenReady.isValid()) m_screenReady.start();
        if (sourcesReady()) {
            // Save the restore token now, not at the end: a crash later must not cost the user another picker.
            saveScreenChoice();
            QString error;
            if (!startOutput(m_spec, &error)) { fail(error); return; }
            m_phase = Running;
            emit started();
        } else if (m_needCamera && !m_testCamera && m_waited.elapsed() > 15000 && (!m_needScreen || (m_screenReady.isValid() && m_screenReady.elapsed() > 15000))) {
            // The picker (if any) is done, so only the camera is missing: say that, not "nothing was chosen".
            fail("the camera did not start: is it plugged in and not in use by another program?");
        } else if (m_needScreen && !m_screenReady.isValid() && m_waited.elapsed() > 120000) {
            fail("nothing was chosen in the screen dialog, so recording was cancelled");
        }
    } else if (m_phase == Running || m_phase == Stopping) {
        pollStream();
        const int code = m_stopCode.load();
        if (code != 1 && !obs_output_active(m_out)) {
            if (code != OBS_OUTPUT_SUCCESS) {
                const char *detail = obs_output_get_last_error(m_out);
                const QString why = detail ? QString::fromUtf8(detail) : QString();
                QString message = why.isEmpty() ? QString("recording stopped unexpectedly (output code %1)").arg(code) : why;
                // A take lost to a full disk is usually still a playable file: say where it is, so nobody thinks the whole take is gone.
                if (QFileInfo(m_path).size() > 0) message += QString(". What was recorded so far is in %1").arg(m_path);
                fail(message);
                return;
            }
            const QString done = m_path;
            finishAttempt();
            emit finished(done);
        }
    }
}

void ObsBackend::outputStopped(void *self, calldata *data) {
    // libobs signals from its output thread. poll() does cleanup and emits UI signals on Qt's thread.
    static_cast<ObsBackend *>(self)->m_stopCode = int(calldata_int(data, "code"));
}

void ObsBackend::pause(bool paused) {
    if (m_phase == Running && m_out) obs_output_pause(m_out, paused);
}

void ObsBackend::end() {
    if (m_phase == Waiting) {          // cancelled before any frame: close the portal session cleanly
        finishAttempt();
        emit finished(QString());
    } else if (m_phase == Running) {
        m_phase = Stopping;
        m_streamAsked = true;
        if (m_stream) obs_output_stop(m_stream);   // the broadcast ends now; the file below may take a moment longer
        obs_output_stop(m_out);       // finishes asynchronously; poll() reports when the file is complete
    }
}

// ---------------------------------------------------------------- the preview

void ObsBackend::setPreview(bool on, const RecordSpec &spec) {
    if (!on) {
        if (m_previewOn) MobsLog::write("capture", "preview off");
        m_previewOn = false;
        removePreviewCallback();
        if (m_phase == Off) dropGraph();
        return;
    }
    QString error;
    if (!ensureStarted(&error) || !ensureGraph(spec, &error)) {
        MobsLog::write("capture", "preview failed: " + error);
        m_previewOn = false;
        removePreviewCallback();
        emit previewFailed(error);
        return;
    }
    if (!m_previewOn) MobsLog::write("capture", QString("preview on (pick %1)").arg(spec.pick));
    m_previewOn = true;
    m_previewSpec = spec;
    addPreviewCallback();
}

void ObsBackend::addPreviewCallback() {
    if (m_previewCb) return;
    video_scale_info info{};
    info.format = VIDEO_FORMAT_BGRA;                 // byte order B,G,R,A = Qt's Format_RGB32 on little endian
    info.width = 640; info.height = 360;
    info.range = VIDEO_RANGE_DEFAULT; info.colorspace = VIDEO_CS_DEFAULT;
    obs_add_raw_video_callback2(&info, 2, &ObsBackend::rawFrameCb, this);   // divisor 2: 15 frames per second is plenty
    m_previewCb = true;
}

void ObsBackend::removePreviewCallback() {
    if (!m_previewCb) return;
    obs_remove_raw_video_callback(&ObsBackend::rawFrameCb, this);
    m_previewCb = false;
}

void ObsBackend::rawFrameCb(void *self, video_data *frame) { static_cast<ObsBackend *>(self)->onRawFrame(frame); }

// Runs on libobs's video thread. Copy the picture, keep only the newest, and tell the main thread at most once per frame.
void ObsBackend::onRawFrame(video_data *frame) {
    if (!frame || !frame->data[0]) return;
    QImage img(640, 360, QImage::Format_RGB32);
    for (int y = 0; y < 360; ++y)
        memcpy(img.scanLine(y), frame->data[0] + size_t(y) * frame->linesize[0], 640 * 4);
    { QMutexLocker l(&m_frameMutex); m_latest = std::move(img); }
    if (!m_frameQueued.exchange(true)) {
        QMetaObject::invokeMethod(this, [this] {
            m_frameQueued = false;
            QImage copy;
            { QMutexLocker l(&m_frameMutex); copy = m_latest; }
            if (m_previewCb) emit previewFrame(copy);   // late frames after the preview was switched off are dropped
        }, Qt::QueuedConnection);
    }
}
