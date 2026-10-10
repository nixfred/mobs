#include "obsbackend.h"
#include "placement.h"
#include <obs.h>
#include <obs-nix-platform.h>
#include <QFile>
#include <QGuiApplication>
#include <QtGui/qguiapplication_platform.h>
#include <QDebug>
#include <linux/videodev2.h>
#include <string>

namespace {
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
}

ObsBackend::ObsBackend(QString tokenFile, QObject *parent) : RecorderBackend(parent), m_tokenFile(std::move(tokenFile)) {
    m_poll.setInterval(100);
    connect(&m_poll, &QTimer::timeout, this, &ObsBackend::poll);
}

ObsBackend::~ObsBackend() {
    m_poll.stop();
    teardown();
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
    if (!obs_startup("en-US", nullptr, nullptr)) { *error = "the recording engine would not start"; return false; }
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
    oai.samples_per_sec = 48000; oai.speakers = SPEAKERS_STEREO;
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

static void fillCanvas(obs_scene_item *item) {
    vec2 bounds{float(Placement::kCanvasW), float(Placement::kCanvasH)};
    obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_INNER);
    obs_sceneitem_set_bounds(item, &bounds);
}

bool ObsBackend::buildGraph(const RecordSpec &spec, QString *error) {
    m_scene = obs_scene_create("mobs");
    m_needScreen = spec.pick != 3;
    m_needCamera = spec.pick != 1;
    m_testScreen = qEnvironmentVariable("MOBS_TEST_SOURCE") == "colour";    // tests record without the picker
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
            const QString token = readToken(m_tokenFile);
            if (!token.isEmpty()) obs_data_set_string(ps, "RestoreToken", token.toUtf8().constData());
            m_video = obs_source_create("pipewire-screen-capture-source", "my screen", ps, nullptr);
            obs_data_release(ps);
            if (token.isEmpty()) emit waitingForUser("choose a screen or a window in the dialog that just opened (a dragged region can record black)");
        }
        if (!m_video) { *error = "could not create the screen source (is the PipeWire plugin installed?)"; return false; }
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
        if (!m_camera) { *error = "could not create the camera source (is the v4l2 plugin installed?)"; return false; }
        m_camItem = obs_scene_add(m_scene, m_camera);   // added second = drawn on top of the screen
        if (spec.pick == 3) {
            fillCanvas(m_camItem);                      // just my camera: it fills the frame
        } else {
            const QRect r = Placement::cameraRect(spec.camera.corner, spec.camera.size);
            vec2 pos{float(r.x()), float(r.y())}, bounds{float(r.width()), float(r.height())};
            obs_sceneitem_set_pos(m_camItem, &pos);
            obs_sceneitem_set_bounds_type(m_camItem, OBS_BOUNDS_SCALE_INNER);
            obs_sceneitem_set_bounds(m_camItem, &bounds);
        }
    }
    obs_set_output_source(0, obs_scene_get_source(m_scene));

    m_mic = obs_source_create("pulse_input_capture", "mic", nullptr, nullptr);   // default input device
    if (m_mic) { obs_source_set_audio_mixers(m_mic, 1); obs_set_output_source(1, m_mic); }

    obs_data_t *vs = obs_data_create();
    obs_data_set_string(vs, "rate_control", "CRF"); obs_data_set_int(vs, "crf", 23);
    m_venc = obs_video_encoder_create("obs_x264", "mobs video", vs, nullptr);
    obs_data_release(vs);
    obs_data_t *as = obs_data_create();
    obs_data_set_int(as, "bitrate", 160);
    m_aenc = obs_audio_encoder_create("ffmpeg_aac", "mobs audio", as, 0, nullptr);
    obs_data_release(as);
    if (!m_venc || !m_aenc) { *error = "no video or audio encoder is available"; return false; }
    obs_encoder_set_video(m_venc, obs_get_video());
    obs_encoder_set_audio(m_aenc, obs_get_audio());

    obs_data_t *os = obs_data_create();
    obs_data_set_string(os, "path", spec.path.toUtf8().constData());
    m_out = obs_output_create("mp4_output", "mobs recording", os, nullptr);
    obs_data_release(os);
    if (!m_out) { *error = "could not create the file output"; return false; }
    obs_output_set_video_encoder(m_out, m_venc);
    obs_output_set_audio_encoder(m_out, m_aenc, 0);
    return true;
}

bool ObsBackend::sourcesReady() const {
    const bool screenOk = !m_needScreen || m_testScreen || obs_source_get_width(m_video) > 0;
    const bool cameraOk = !m_needCamera || m_testCamera || obs_source_get_width(m_camera) > 0;
    return screenOk && cameraOk;
}

void ObsBackend::fail(const QString &why) {
    m_poll.stop();
    teardown();
    m_phase = Off;
    emit failed(why);
}

void ObsBackend::begin(const RecordSpec &spec) {
    QString error;
    if (!ensureStarted(&error) || !buildGraph(spec, &error)) {
        fail(error);
        return;
    }
    m_path = spec.path;
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
            if (m_needScreen && !m_testScreen) {
                obs_source_save(m_video);
                obs_data_t *st = obs_source_get_settings(m_video);
                const QString tok = QString::fromUtf8(obs_data_get_string(st, "RestoreToken"));
                obs_data_release(st);
                if (!tok.isEmpty()) writeToken(m_tokenFile, tok);
            }
            if (!obs_output_start(m_out)) {
                const QString why = QString::fromUtf8(obs_output_get_last_error(m_out));
                fail(why.isEmpty() ? "the recording would not start" : why);
                return;
            }
            m_phase = Running;
            emit started();
        } else if (m_needCamera && !m_testCamera && m_waited.elapsed() > 15000 && (!m_needScreen || (m_screenReady.isValid() && m_screenReady.elapsed() > 15000))) {
            // The picker (if any) is done, so only the camera is missing: say that, not "nothing was chosen".
            fail("the camera did not start: is it plugged in and not in use by another program?");
        } else if (m_needScreen && !m_screenReady.isValid() && m_waited.elapsed() > 120000) {
            fail("nothing was chosen in the screen dialog, so recording was cancelled");
        }
    } else if (m_phase == Stopping) {
        if (!obs_output_active(m_out)) {
            m_poll.stop();
            const QString done = m_path;
            teardown();
            m_phase = Off;
            emit finished(done);
        }
    }
}

void ObsBackend::pause(bool paused) {
    if (m_phase == Running && m_out) obs_output_pause(m_out, paused);
}

void ObsBackend::end() {
    if (m_phase == Waiting) {          // cancelled before any frame: close the portal session cleanly
        m_poll.stop(); teardown(); m_phase = Off;
        emit finished(QString());
    } else if (m_phase == Running) {
        m_phase = Stopping;
        obs_output_stop(m_out);       // finishes asynchronously; poll() reports when the file is complete
    }
}

void ObsBackend::teardown() {
    // Order matters (spike 4): output, encoders, detach the scene item, sources, scene, then wait for libobs.
    if (m_out) { obs_output_release(m_out); m_out = nullptr; }
    if (m_venc) { obs_encoder_release(m_venc); m_venc = nullptr; }
    if (m_aenc) { obs_encoder_release(m_aenc); m_aenc = nullptr; }
    if (m_libobsUp) { obs_set_output_source(0, nullptr); obs_set_output_source(1, nullptr); }
    if (m_item) { obs_sceneitem_remove(m_item); m_item = nullptr; }
    if (m_camItem) { obs_sceneitem_remove(m_camItem); m_camItem = nullptr; }
    if (m_mic) { obs_source_release(m_mic); m_mic = nullptr; }
    if (m_camera) { obs_source_release(m_camera); m_camera = nullptr; }
    if (m_video) { obs_source_release(m_video); m_video = nullptr; }
    if (m_scene) { obs_scene_release(m_scene); m_scene = nullptr; }
    if (m_libobsUp) obs_wait_for_destroy_queue();
}
