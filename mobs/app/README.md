# MOBS app

MOBS's own window (ADR 0005). Qt 6 Quick, GPLv2. It reuses Monologue's `theme.cpp` **by path** from
`../../monologue/src`, so subtree pulls keep updating it.

**What works now:** the choose, ready and recording screens; the live Omarchy theme (full palette, Hyprland rounding,
theme switches, contrast guard); your pick remembered; the real display name; and **real recording with libobs inside
this process** (ADR 0006): Space or the red button starts, Space pauses and resumes, Enter stops and saves an MP4
(H.264 + AAC) under `~/Videos/mobs`. The first screen recording opens the system's screen picker once; MOBS saves the
restore token itself (`~/.config/mobs/screen-token`), so later recordings are silent. **Cameras (2026-10-10):** the camera chip on the ready screen opens a panel: switch between *my screen*, *screen + me* and
*just my camera*; pick a webcam from the ones found under `/dev/v4l/by-id` (named from the system; the stable id survives
`/dev/videoN` renumbering); for *screen + me*, tap a corner of a mini screen and choose small, medium or large. The
diagram and the recording use the same rectangle (`src/placement.h`). The choice is remembered. MOBS asks the camera for 1280x720
MJPEG when it supports it.

**Live preview (2026-10-10):** the *preview* chip on the ready screen shows what will be recorded, live (15 frames per second, 640x360):
your screen, and your webcam where you placed it. It follows the choices as you make them (tap a corner and the webcam moves in the
preview), keeps running during a recording, and is **off by default and remembered**, because it turns the camera on and may open
the screen dialog. Frames come out of libobs as plain pictures (`obs_add_raw_video_callback2`), not a native surface inside the
window, which is simple, robust on Wayland, and testable by reading pixels.

**Review (2026-10-10):** when a recording is saved, MOBS opens it for review by itself: watch it (plays only the parts you keep,
hopping over cuts), and trim and cut on **Monologue's own timeline editor** (`EditBar.qml`, used unmodified from the subtree): drag
a handle to trim, double-click to split, click x to cut a part out, with a white mark wherever you paused. **save edit** writes
`<name> (edited).mp4` beside the original (never overwriting, never touching the original; encoded to a hidden partial file and
renamed into place); **done** leaves (asking first if there are unsaved edits); **delete take** moves it to the trash (asks first).
Keys: Space play, S split, X cut or restore, Left/Right and Shift/Alt nudges, [ ] jump between marks, Z zoom, Ctrl+Z / Ctrl+Y undo and redo,
Ctrl+S save, Esc done. The pause marks come from `<take>.mobs.json`, written when you stop recording.

**EXPERT:** the header button (or P) opens focused recording, encoding, audio, keys, about and hardware pages. Choose a plain quality preset or set custom x264 CRF / CPU preset; change the save folder explicitly with **use folder**. Software encoding is the default; the hardware page checks NVIDIA H.264 availability before you choose it. NVIDIA uses CQP quality and the p5 preset. Settings apply to the next recording. Closing during a recording stops and saves; unsaved edits and active exports are protected. See [the usability and reliability report](../../docs/quality-and-expert.md) for fixes, tests and the next settings to add.

**Audio:** ready shows the chosen microphone, a mic toggle and a desktop-sound toggle. **EXPERT audio** selects a named input or output, offers an explicit **check mic** with a live meter, and has an **EXPERT gain** page for independent −60 to +12 dB adjustments. Mic checking captures without saving or playing sound. Desktop levels appear during recording; both sources mix into one AAC track. Microphone defaults on at 0 dB, desktop off at −6 dB. Device discovery requires `pactl` and a compatible PulseAudio service (including PipeWire's PulseAudio service). Missing inputs block recording with an explanation; turning both inputs off permits a silent take.

Review loading keeps the window responsive and explains inspection failures on Ready. If saving a recording stalls, close the window again to choose **keep waiting** or **quit without waiting**. Quitting without waiting may leave that recording incomplete or unplayable; normal close still waits for a save.

The editor keeps picture warnings and export failures visible together and fits its confirmations in a small window. Camera selection uses arrows to browse named devices, keeping placement controls and Done visible even with several cameras connected.

Architecture: `Engine` (state machine for QML) drives a `RecorderBackend` (a narrow interface: begin, pause, end).
The only real backend is `ObsBackend` (libobs); tests use a fake one. Keeping that door small is what would let the engine
be swapped for another language later (Fred chose C++17 + QML first, 2026-10-09).

## Seeing what is being recorded

While a take is recording or paused, the recording screen shows **the live picture MOBS is saving**, whether or not the ready screen's
"preview" chip was on. It is the same scene the encoder reads (screen, plus your camera in its corner, or just the camera), at 15 frames a
second. A red outline and the line "recording: this is what is being saved" tell you it is live; **pause** dims it and says "paused: nothing is being
saved"; while the file is finished it stays, dimmed, under "saving...". Wide windows put the picture beside the timer and buttons, tall
ones put it above them; nothing scrolls (Law 17).

- The rule lives in `Engine::applyPreview`: preview = (what the ready screen asked for) OR (recording or paused). It is applied on the change to
  or from recording, never while MOBS is still waiting for the screen picker (so a second copy of the scene cannot race the recording's own).
- The ready screen's own choice is untouched: if the preview was on before the take it is on after, if it was off it goes back off (and the scene,
  with the camera, is released as soon as the take is saved).
- **Recording the screen MOBS is on shows a tunnel.** If the captured screen includes MOBS's own window, the picture shows MOBS showing the picture, and so on.
  That is what is actually being recorded. Pick a window, or keep MOBS on another monitor or workspace, to avoid it.
- The picture is 640x360 (a monitor, not a magnifier): enough to see layout, camera placement and whether anything is black.

![recording, wide](../../docs/app/recording-preview-wide.png)
![paused](../../docs/app/recording-preview-paused.png)
![the real libobs backend in the practice VM: screen colour, "me" box in the corner](../../docs/app/recording-preview-real-libobs.png)

## The opening screen

MOBS opens on a 2.8 s opening screen (`qml/SplashScreen.qml`), then fades into the app. One clock drives it, so every frame is a pure
function of time: the three viewfinder corners snap in, the record ring draws itself round with a bright head, the dot goes live
(pop, shockwave, then it breathes), the webcam lens opens in the corner where MOBS puts your camera and blinks, `MOBS` decodes letter by letter
with a cyan/red split as each letter lands, and a readout reports what this machine actually has: the libobs version, the display, the cameras found,
the microphone, and where recordings go. Every colour comes from the live Omarchy theme (checked on light, neon and orange themes).

- **Any key or click skips it.** The key is eaten, so a Space pressed during the opening screen can never start a recording.
- **A recording that starts (the hotkey launching MOBS) dismisses it at once.**
- **Off switch:** EXPERT > recording > *play the opening animation* (decided at launch; flipping it later never makes it appear mid-session).
- Scripted runs and `MOBS_GRAB` screenshots skip it, so they land on the real screens. For its own checks: `MOBS_SPLASH=1` plays it in a scripted run,
  `MOBS_SPLASH_AT=MS` freezes it at that moment (design checks, screenshots), `MOBS_SPLASH=0` forces it off.

![the opening screen, six moments](../../docs/app/splash-sequence.png)
![the finished opening screen](../../docs/app/splash-final.png)
![on a light, a neon and an orange Omarchy theme](../../docs/app/splash-themes.png)

## Install: launcher entry, icon, hotkey launch

```sh
cmake --install build --prefix ~/.local     # binary, launcher entry, icon, D-Bus launch file
```

MOBS then shows up in the launcher (SUPER+SPACE, Omalaunch reads `.desktop` files live) as **MOBS** with its icon
(`dist/mobs.svg`: a record ring and dot, viewfinder corners, and a webcam lens in the bottom-right where MOBS puts your
webcam). Right-clicking offers *Start or stop recording* and *Pause or resume*. A test installs into a temporary folder
and checks the paths and that the installed binary starts and draws, because an earlier build only worked next to its
build folder. Re-run the command after rebuilding.

**Getting back to the three choices (my screen / screen + me / just my camera):** click **change** next to the mode text on the
ready screen, press **Esc**, or click the camera chip. They are shown full-screen on first run only.

## The hotkey (physical Alt + Shift + V)

MOBS exports `org.mobs.Control` on the session bus: `Toggle` (start, or stop and save), `Pause` (pause or resume) and
`Status`. A Hyprland bind runs one command from any application:

```sh
busctl --user call org.mobs.Control /org/mobs/Control org.mobs.Control Toggle     # SUPER + SHIFT + V  (physical Alt + Shift + V)
busctl --user call org.mobs.Control /org/mobs/Control org.mobs.Control Pause      # SUPER + CTRL + SHIFT + V
```

If MOBS is not running, D-Bus starts it: copy `dist/org.mobs.Control.service` to `~/.local/share/dbus-1/services/`, replace
`@MOBS_BIN@` with the path of the `mobs` binary, and reload the bus once (or log in again). Proven in the Test Drive VM:
Toggle, Pause, Pause, Toggle made a valid file with the pause cut out; auto-launch started MOBS in 0.5 s; a second copy does not take
the hotkey over. (`MOBS_NO_HOTKEY=1` stops a copy from registering.)

```sh
cmake -S . -B build -G Ninja && cmake --build build
QT_QPA_PLATFORMTHEME=generic QT_QUICK_BACKEND=software QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure   # unit tests, no window appears
# real libobs recording test (needs a Wayland session, e.g. the Test Drive VM):
QT_QPA_PLATFORM=wayland MOBS_LIBOBS_TEST=1 ./build/tst_obsbackend
./build/mobs
```

Native integration runs each scenario in a separate process, matching the app's single libobs lifetime. The installed OBS NVIDIA plugin cannot reliably unload and reload in one process; repeated takes and NVIDIA/software switching are tested within a single backend. CMake copies and installs the system `obs-nvenc-test` helper alongside MOBS when available, as the NVIDIA plugin expects it there.

Test and screenshot switches (env vars): `MOBS_THEME_DIR` (a fake Omarchy "current" folder), `MOBS_SETTINGS` (settings file),
`MOBS_SCREEN=1` / `MOBS_PICK=1..3` (start screen), `MOBS_WINDOW_SIZE=860x560`, `MOBS_GRAB=/path.png` (save the window's own pixels and quit), `MOBS_TEST_SOURCE=colour` (record a test colour instead of the screen, no picker), `MOBS_REC_DIR`, `MOBS_TOKEN_FILE`, `MOBS_TEST_CAMERA=colour` (a green stand-in webcam), `MOBS_FAKE_CAMERAS=1` (two sample cameras for screenshots), `MOBS_NO_HOTKEY=1`, `MOBS_VERBOSE=1` (libobs logs warnings and errors only otherwise). Script steps also include `pick=N`, `corner=N`, `size=N`, `panel=0/1`, `preview=0/1`, `grabpreview=`, `review=`, `split=`, `cut=`, `savereview`, `waitsaved`, `closereview`, `pro=0/1`, `tab=0..5`, `checklayout`, `key=P/Space/Esc`, `focusfolder`, `foldertext=`, `applyfolder`, `closewindow`, and `expect=state:idle` (also `pro`, `screen`, `confirmQuit`, `folderDefault`). Script `quit` explicitly exits without asking about unsaved test edits.

Script condition `expect=previewing:0|1` reads whether the live picture is running (the fake backend serves a flat colour frame for it).
Opening-screen switches: `MOBS_SPLASH=1|0`, `MOBS_SPLASH_AT=MS` (see "The opening screen"); the `expect=splashDone:0|1` script condition reads whether it has finished.
Audio test switches: `MOBS_FAKE_AUDIO=1` lists demo inputs; `MOBS_TEST_AUDIO=file:/path.wav` and `MOBS_TEST_DESKTOP_AUDIO=file:/path.wav` supply generated sound to native tests. `MOBS_FAKE_BACKEND=1` is accepted only with `MOBS_SCRIPT` and exercises QML without capture. EXPERT tabs are `tab=0..5`; scripts also support `openaudio`, `audiodetails=0/1`, `micdevice=`, `desktop=0/1`, `miccheck=0/1`, `micgain=`, and `hardware=0/1`.

Layout: `src/palette.*` (theme reader), `src/appstate.*` (screens and pick), `qml/` (components and screens), `tests/`.
Screens are checked in the Test Drive practice VM, never on a live desktop; see `docs/app/`.
