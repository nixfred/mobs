# MOBS app (skeleton)

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

**Not wired yet (labelled on screen):** the review screen and the Pro drawer.

Architecture: `Engine` (state machine for QML) drives a `RecorderBackend` (a narrow interface: begin, pause, end).
The only real backend is `ObsBackend` (libobs); tests use a fake one. Keeping that door small is what would let the engine
be swapped for another language later (Fred chose C++17 + QML first, 2026-10-09).

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
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure   # unit tests, no window appears
# real libobs recording test (needs a Wayland session, e.g. the Test Drive VM):
QT_QPA_PLATFORM=wayland MOBS_LIBOBS_TEST=1 ./build/tst_obsbackend
./build/mobs
```

Test and screenshot switches (env vars): `MOBS_THEME_DIR` (a fake Omarchy "current" folder), `MOBS_SETTINGS` (settings file),
`MOBS_SCREEN=1` / `MOBS_PICK=1..3` (start screen), `MOBS_GRAB=/path.png` (save the window's own pixels and quit), `MOBS_TEST_SOURCE=colour` (record a test colour instead of the screen, no picker), `MOBS_REC_DIR`, `MOBS_TOKEN_FILE`, `MOBS_TEST_CAMERA=colour` (a green stand-in webcam), `MOBS_FAKE_CAMERAS=1` (two sample cameras for screenshots), `MOBS_NO_HOTKEY=1`. Script steps also include `pick=N`, `corner=N`, `size=N`, `panel=0/1`.

Layout: `src/palette.*` (theme reader), `src/appstate.*` (screens and pick), `qml/` (components and screens), `tests/`.
Screens are checked in the Test Drive practice VM, never on a live desktop; see `docs/app/`.
