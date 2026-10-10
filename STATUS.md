# mobs: status

- Started: 2026-10-09, from Clarity's Idea Box
- Stage: product architecture decided 2026-10-09 (ADR 0006); app skeleton (mobs/app) built; spike 4 proved libobs in-process recording.
- Repo: github.com/nixfred/mobs (private). OBS and Monologue are git subtrees (obs/, monologue/) with full history and the official upstreams as remotes (ADR 0002).
- Finding: Monologue is a webcam recorder with a trim editor (Qt 6 QML, C++), not a voice tool. Voxtype (already on Omarchy) is the dictation tool.

## Decided (Fred, 2026-10-09; details in README.md)

- For: Fred only, on his Omarchy machines (gus, vic).
- Destination: all of OBS's and Monologue's features in one app. Left out: Windows and macOS, voice dictation and transcripts. Not ruled out: live streaming, packaging.
- Hotkey: physical Alt + Shift + V starts/stops (pause: Ctrl + Alt + Shift + V). Alt + Shift + R was taken (Rename workspace).
- Interface: "super easy for beginners, satisfy experts, not too much like OBS, Omarchy feel." MOBS is its own application in the live Omarchy theme. **Fred, later the same day: "I don't want an overlay. I want a new product."** So MOBS is one program with libobs (the OBS engine) embedded in-process, no OBS app, no websocket, nothing hidden (ADR 0006, replaces ADR 0005 and 0003). Beginner: three cards then one Record button. Expert: a Pro drawer that grows to cover what OBS can do.

## Evidence

- Spike 1 and 2 (ADR 0004, mobs/spike/): Monologue's real EditBar.qml runs in an OBS dock on native Wayland with Qt Quick software rendering (OpenGL turned OBS's preview black, A/B in the VM); real video plays via QVideoSink to QImage, also on gus's real hardware. But OBS exits cleanly only about half the time with any QML dock loaded on gus (no plugin 6/6, plugin without dock 6/6, empty QML dock 4/8 stuck, full dock 3/6; one cause found, the "Closing Dockable Window" dialog, the rest unexplained). That is why MOBS left OBS's window.
- Screen prototype (mobs/design/, docs/design/screen-1..5.png, docs/screen-design.md): five screens in the live Liftoff theme, rendered in the Test Drive VM with sample data only. Screen 4 uses Monologue's EditBar.qml unmodified.
- Practice copy: Test Drive VM test-drive-mobs-spike (OBS 32.2.2, Qt 6.11.2, cmake/ninja; its package DB was stale, refreshed with pacman -Sy). Still running.

- Spike 3 (mobs/spike3/, ADR 0005): PASSED for the control plane in the VM. OBS hidden on special:mobs, websocket auth, scene/input, record, pause (cut 2 s of a 7 s run), resume, stop, file path back, OBS alive throughout. Learned: RecQuality must not be Stream or pause does nothing; wait for the RecordStateChanged STOPPED event, the file is not final when StopRecord replies.

## Spike 4 (mobs/spike4, ADR 0006): PASSED in the VM

A Qt program with libobs embedded recorded a valid file with pause, on native Wayland, no OBS application running: 12 of 13 engine plugins (NVENC absent in the VM), x264 + AAC, mp4_output, 4.8 s from 3 s + 2 s paused + 2 s, 16 of 16 clean exits after three fixes: set the Wayland platform before obs_startup, load engine plugins by name (never obs_load_all_modules), and obs_wait_for_destroy_queue before obs_shutdown. Spike 3 (hidden OBS over websocket) is superseded; its portal findings stand.

Spike 4b, real gus hardware: in-process PipeWire screen capture works. First run opened the picker (Fred picked), recording with pause had real content (1,871 colours); MOBS saved the restore token itself and the second run needed no picker (2,279 colours). Watch: "Cannot create EGLImage" warnings (fallback worked).

## Engine and real recording (mobs/app, 2026-10-09): works in the VM

`Engine` (state machine) + `RecorderBackend` interface + `ObsBackend` (libobs in-process, RAII-ordered teardown, non-blocking start while the picker is open, restore token saved the moment the portal answers). 15 engine tests with a fake backend (a deliberate bug in the pause logic was first NOT caught, so I strengthened the test and re-proved it fails), 3 real-libobs integration tests on Wayland (record + pause with a duration check, record twice in one process, cancel before start). Driven like a user in the VM with real key presses (choose, Space, Space, Space, Enter): a 1920x1080 H.264 + AAC file of 6.40 s from 6.81 s key-timed recording with a 2.24 s pause cut out. Run on gus's real screen through the app on 2026-10-10 (see below).

## Review screen (2026-10-10): built, tested, and run through the whole loop in the VM

After a recording is saved the app opens it for review: playback that skips cut gaps, Monologue's UNMODIFIED EditBar.qml (mapped into the module from the subtree; type name and qml/ directory needed explicit CMake properties), a `Review` object that mirrors Monologue's editing rules (a drag is ONE undo step; clips grow into gaps never over neighbours; the last clip can't be removed; atomic export to a hidden partial file), save writes `<name> (edited).mp4` beside the original, delete goes to the TRASH, leaving asks first. The engine writes `<take>.mobs.json` with the pause points so the timeline shows where you paused. 19 review tests with real ffmpeg (export keeps ~4 s of 6 with audio, original byte-for-byte unchanged, no partial files left, a vanished source fails cleanly and stays unsaved, delete lands in a temp trash), engine sidecar tests, app-state tests. VM run through the app's own script: record 2 s + pause 1 s + 3 s, review opened by itself, split at 1.5 and 3.5 s, cut the middle, saved: take 5.0 s, sidecar pause at 2.085 s, edited 2.97 s, original still 5.0 s. Also fixed: MOBS now logs only warnings and errors from libobs (a running MOBS was writing ~60 debug lines per second to the user journal); install test now also catches a binary that cannot find its own QML types.

## Live preview (2026-10-10): built and tested in the VM

The `preview` chip on the ready screen shows what will be recorded. ObsBackend now has two lifetimes: the GRAPH (scene + screen/camera sources, lives while previewing or recording) and the RECORDING (mic, encoders, file, only while recording), so a recording reuses the preview's scene and a second screen-share dialog never opens. Frames: `obs_add_raw_video_callback2` (BGRA, 640x360, divisor 2 = 15 fps) copied on libobs's video thread, newest only, handed to the Qt thread, served to QML through an image provider. 14 real-libobs tests in the VM (all pass), new ones read the pixels of preview frames: frames arrive repeatedly; the webcam sits in the chosen corner; tapping another corner/size moves it in the SAME scene; switching the preview off really stops frames; a recording while previewing produces a file with the same picture, the preview keeps running during and after; two recordings in a row clean up. A deliberate bug (placement not updated) was caught by the placement test on the exact assertion. Unit: 8 suites pass on gus (engine preview rules incl. camera unplugged, saved setting default off, model thread-safety). NOT yet run with the real screen picker or the real webcam preview on gus; ask Fred first (turns the camera on).

## Launcher, icon and hotkey bind installed on gus (2026-10-10)

`cmake --install build --prefix ~/.local` puts MOBS in the launcher with a new icon (mobs/app/dist/mobs.svg), and installs the D-Bus launch file. Verified: desktop file valid; GIO sees it (name MOBS, Exec correct); the icon resolves by name at 32/48/128 px under the Yaru-blue theme; launching through the entry (gtk-launch) opened a window of class `mobs`; with MOBS closed the hotkey's busctl call started it in 0.6 s. Bugs found on the way, each now guarded by a test: the installed Exec/TryExec had the default prefix (/usr/local/bin) because CMake filled it at configure time, so the launcher would have hidden MOBS (now filled at install time); the INSTALLED binary exited with "Module Mobs contains no type named Main" because the QML module's qmldir said `prefer :/Mobs/` and only worked next to the build folder (fixed with qt_policy QTP0001 NEW and a separate module name for the C++ types; ctest now installs to a temp prefix and RUNS that binary). Hyprland binds added to ~/.config/hypr/bindings.lua as a marked BEGIN/END block (backup bindings.lua.bak-mobs-hotkey-20261010): SUPER+SHIFT+V toggle, SUPER+CTRL+SHIFT+V pause (physical Alt+Shift+V); verified registered, no config errors, clipboard-policy check all OK. Side effect to know: the reload dropped the live bind count 303 -> 291 (runtime binds that a config reload clears; with my block it is 293; the 12 are unidentified because I did not snapshot first). Also added: a visible "change" pill on the ready screen (the three choices were only reachable by Esc or the camera chip).

## Real webcam run on gus (2026-10-10 00:2x)

Logitech BRIO through MOBS (script mode, no keys): negotiated MJPEG 1280x720 at 30 fps. Screen + me (bottom-right, medium): 5.30 s, 1920x1080 H.264 + AAC, camera box has a real picture (813 colours) and the rest is real screen content (1637 colours; Fred picked a screen/window, not a region). Camera only: recorded fine, but the picture covered only ~12% of the frame. Cause found by comparing with a native ffmpeg grab of the same camera (46-59% non-black, full width): libobs treated the webcam's FULL-range MJPEG as limited range and crushed dark pixels in a dim room. Fix: color_range = FULL for the v4l2 source; after it MOBS shows 52% non-black spanning the full width vs 59% native. (An async stand-in source filling the frame was checked first to rule out geometry.) Frames judged by numbers only, never viewed; files deleted.

## Cameras (2026-10-10): built and tested in the VM, then run on the real BRIO (above)

Answer to Fred's question "where is the input for my camera and how do I place it": the camera chip on the ready screen opens a panel (which webcam; screen + me corner and size with a tap-a-corner diagram; just my camera fills the frame). gus has two cameras found under /dev/v4l/by-id: Logitech BRIO and HD Webcam. Engine refuses a camera pick with no camera and opens the panel. ObsBackend builds screen / screen + camera / camera-only scenes with libobs's v4l2_input (1280x720 MJPEG when supported) and reports a camera that never starts as a camera problem. 22 engine, 15 app-state, 10 camera/placement tests, plus 9 real-libobs tests in the VM that read the pixels of saved files: the stand-in camera lands in each corner, the opposite corner and the middle stay screen, sizes grow, camera-only fills the frame. Two bugs found by tests/looking and fixed: the serial-number stripper removed the word Webcam from a camera name; the panel diagram did not follow corner/size changes (QML dependency). NOT yet run with a real webcam on gus (turns the camera on and records a face: needs Fred's OK).

## Hotkey (2026-10-10): works over real D-Bus in the VM

`org.mobs.Control` (Toggle, Pause, Status) in the app, 6 unit tests, and a real `busctl` run: Toggle, Pause, Pause, Toggle produced a valid file with the pause cut out; D-Bus auto-launch (after a bus config reload) started MOBS in 0.5 s and the next Toggle recorded; a second copy does not steal the name. NOT yet bound on gus: bindings.lua is Fred's locked live config, so the Hyprland bind waits for his OK (SUPER+SHIFT+V and SUPER+CTRL+SHIFT+V, both free).

## Real run on gus + black-recording warning (2026-10-10)

The app ran on gus with real PipeWire capture via its own test script (no synthetic keys): clean exits, silent second run with the saved token, pause cut 2 s out of a 7 s run. Fred picked a REGION and the video was black, the third run where a region pick did that, so MOBS now checks each saved file and warns in red ("pick a screen or a window"); the picker prompt says the same. 19 engine tests (real ffmpeg black/normal videos); a signed-byte bug in the first version was caught by a test. The leftover "full obs" door was removed (there is no OBS app any more). Still to see: a real run with a screen or window pick through the app (expect real frames).

## App skeleton (mobs/app, 2026-10-09)

- Choose and ready screens in Qt Quick, split into components from the prototype; the live Omarchy palette (full colors.toml + Hyprland rounding via Monologue's theme.cpp, reused by path), remembered pick, real display name. 18 unit tests pass (palette parsing, live theme switch, theme folder replaced, contrast guard, app state).
- Proven in the VM: one running process changed from tokyo-night to Catppuccin Latte when the real Omarchy theme was switched (docs/app/live-*.png). Found and fixed a real problem: Tokyo Night's own dim text colours measure 1.6 to 2.4:1; the palette now lifts dim text to 4.5:1 (rendered card text measured 4.65:1).
- Not wired, and labelled on screen: recording, hotkey, mic/camera, Pro drawer, full OBS, the OBS link.

## Open

- Wayland screen capture is PROVEN end to end on gus (ADR 0005, spike 3b): pre-seeded screen source, hidden OBS, one picker click, real frames, then a silent rerun with the saved restore token. Two cautions: a screen source created live over websocket never opens the picker (load it from a scene collection file instead), and a REGION selection recorded black frames (two runs) while another selection worked. Next to test: loading the collection with SetCurrentSceneCollection.
- New dependency: qt6-websockets (not on gus yet).
- Not built: the Hyprland bind itself on gus (needs Fred's OK),   the Pro drawer and its tabs,  the recording pill, the first-run permission screen, a real run on gus with a screen/window pick.
- Throwaway test configs for gus runs live in the Claude scratchpad, not in ~/.config/obs-studio (Fred's own OBS setup was never touched).
