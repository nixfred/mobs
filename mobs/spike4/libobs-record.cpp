// Embed libobs in a Qt program and record 3 s + 2 s paused + 2 s to a file. Copies the init order of OBS's
// own frontend (obs/frontend/OBSApp.cpp): obs_startup, platform + display, reset video/audio, load modules.
#include <obs.h>
#include <obs-nix-platform.h>
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QtGui/qguiapplication_platform.h>
#include <cstdarg>
#include <cstdio>
#include <string>

static void logHandler(int level, const char *fmt, va_list args, void *) {
    if (level > LOG_WARNING && !qEnvironmentVariableIsSet("MOBS_VERBOSE")) return;
    std::vprintf(fmt, args);
    std::printf("\n");
}
static void sleepMs(int ms) { QEventLoop l; QTimer::singleShot(ms, &l, &QEventLoop::quit); l.exec(); }
static int failures = 0;
static void check(bool ok, const char *what) { std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what); std::fflush(stdout); if (!ok) ++failures; }

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    const QByteArray out = qgetenv("MOBS_OUT").isEmpty() ? QByteArray("/tmp/mobs-libobs.mp4") : qgetenv("MOBS_OUT");
    base_set_log_handler(logHandler, nullptr);

    // The platform and display MUST be set before obs_startup (OBS frontend does the same in OBSApp::OBSInit).
    if (QGuiApplication::platformName().contains("wayland")) {
        auto native = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
        obs_set_nix_platform_display(native->display());
        obs_set_nix_platform(OBS_NIX_PLATFORM_WAYLAND);
        std::printf("platform: Wayland\n");
    }
    check(obs_startup("en-US", nullptr, nullptr), "obs_startup");
    obs_add_data_path("/usr/share/obs/libobs/");
    obs_add_module_path("/usr/lib/obs-plugins/", "/usr/share/obs/obs-plugins/%module%");

    obs_video_info ovi{};
    ovi.graphics_module = "libobs-opengl";
    ovi.fps_num = 30; ovi.fps_den = 1;
    ovi.base_width = 1920; ovi.base_height = 1080;
    ovi.output_width = 1280; ovi.output_height = 720;
    ovi.output_format = VIDEO_FORMAT_NV12;
    ovi.colorspace = VIDEO_CS_709; ovi.range = VIDEO_RANGE_PARTIAL;
    ovi.scale_type = OBS_SCALE_BICUBIC;
    ovi.gpu_conversion = true;
    check(obs_reset_video(&ovi) == OBS_VIDEO_SUCCESS, "obs_reset_video (OpenGL via libobs-opengl)");

    obs_audio_info2 oai{};
    oai.samples_per_sec = 48000; oai.speakers = SPEAKERS_STEREO;
    check(obs_reset_audio2(&oai), "obs_reset_audio2");

    // Load ONLY engine plugins. obs_load_all_modules() also loads parts of OBS's own UI (websocket, frontend-tools,
    // decklink-output-ui...), which call a frontend that does not exist in a product with its own frontend.
    static const char *wanted[] = {"obs-x264", "obs-ffmpeg", "obs-outputs", "obs-filters", "obs-transitions",
                                   "image-source", "text-freetype2", "linux-pipewire", "linux-pulseaudio",
                                   "linux-v4l2", "linux-capture", "obs-nvenc", "obs-qsv11"};
    int loaded = 0;
    for (const char *m : wanted) {
        obs_module_t *mod = nullptr;
        const std::string bin = std::string("/usr/lib/obs-plugins/") + m + ".so";
        const std::string data = std::string("/usr/share/obs/obs-plugins/") + m;
        const int rc = obs_open_module(&mod, bin.c_str(), data.c_str());
        if (rc == MODULE_SUCCESS && obs_init_module(mod)) ++loaded;
        else std::printf("  module %-18s not loaded (rc %d)\n", m, rc);
    }
    std::printf("engine modules loaded: %d of %d\n", loaded, (int)(sizeof wanted / sizeof *wanted));
    obs_post_load_modules();

    // scene with a red colour source (no portal needed), plus desktop audio if the pulse plugin gives us one
    obs_scene_t *scene = obs_scene_create("MOBS");
    obs_data_t *cs = obs_data_create();
    obs_data_set_int(cs, "color", 0xFF0000FF); obs_data_set_int(cs, "width", 1920); obs_data_set_int(cs, "height", 1080);
    obs_source_t *colour = obs_source_create("color_source_v3", "red", cs, nullptr);
    check(colour != nullptr, "create colour source");
    obs_sceneitem_t *item = obs_scene_add(scene, colour);   // the scene (not us) owns the item
    obs_set_output_source(0, obs_scene_get_source(scene));
    obs_source_t *audio = obs_source_create("pulse_output_capture", "desktop audio", nullptr, nullptr);
    if (audio) { obs_source_set_audio_mixers(audio, 1); obs_set_output_source(1, audio); }
    std::printf("desktop audio source: %s\n", audio ? "created" : "not available");

    obs_data_t *vs = obs_data_create();
    obs_data_set_string(vs, "rate_control", "CRF"); obs_data_set_int(vs, "crf", 23);
    obs_encoder_t *venc = obs_video_encoder_create("obs_x264", "mobs video", vs, nullptr);
    obs_data_t *as = obs_data_create(); obs_data_set_int(as, "bitrate", 160);
    obs_encoder_t *aenc = obs_audio_encoder_create("ffmpeg_aac", "mobs audio", as, 0, nullptr);
    check(venc && aenc, "x264 + AAC encoders created");
    obs_encoder_set_video(venc, obs_get_video());
    obs_encoder_set_audio(aenc, obs_get_audio());

    obs_data_t *os = obs_data_create(); obs_data_set_string(os, "path", out.constData());
    obs_output_t *rec = obs_output_create("mp4_output", "mobs recording", os, nullptr);
    check(rec != nullptr, "mp4_output created");
    obs_output_set_video_encoder(rec, venc);
    obs_output_set_audio_encoder(rec, aenc, 0);
    check(obs_output_can_pause(rec), "output reports it can pause");

    QElapsedTimer t; t.start();
    check(obs_output_start(rec), "obs_output_start");
    sleepMs(3000);
    check(obs_output_active(rec), "recording active after 3 s");
    check(obs_output_pause(rec, true), "obs_output_pause(true)");
    sleepMs(2000);
    check(obs_output_paused(rec), "output reports paused");
    check(obs_output_pause(rec, false), "obs_output_pause(false)");
    sleepMs(2000);
    obs_output_stop(rec);
    for (int i = 0; i < 100 && obs_output_active(rec); ++i) sleepMs(100);
    check(!obs_output_active(rec), "output stopped");
    std::printf("wall clock %lld ms\nOUTPUT %s\n", (long long)t.elapsed(), out.constData());

    obs_output_release(rec); obs_encoder_release(venc); obs_encoder_release(aenc);
    obs_set_output_source(0, nullptr); obs_set_output_source(1, nullptr);
    if (audio) obs_source_release(audio);
    obs_sceneitem_remove(item);           // detach it first, then drop our own references
    obs_source_release(colour); obs_scene_release(scene);
    obs_data_release(cs); obs_data_release(vs); obs_data_release(as); obs_data_release(os);
    // Releases are queued to libobs's graphics thread; shutting down before they run is a crash.
    obs_wait_for_destroy_queue();
    obs_shutdown();
    std::printf(failures ? "RESULT: %d check(s) FAILED\n" : "RESULT: all checks passed\n", failures);
    return failures ? 1 : 0;
}
