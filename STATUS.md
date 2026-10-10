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

## Real run on gus + black-recording warning (2026-10-10)

The app ran on gus with real PipeWire capture via its own test script (no synthetic keys): clean exits, silent second run with the saved token, pause cut 2 s out of a 7 s run. Fred picked a REGION and the video was black, the third run where a region pick did that, so MOBS now checks each saved file and warns in red ("pick a screen or a window"); the picker prompt says the same. 19 engine tests (real ffmpeg black/normal videos); a signed-byte bug in the first version was caught by a test. The leftover "full obs" door was removed (there is no OBS app any more). Still to see: a real run with a screen or window pick through the app (expect real frames).

## App skeleton (mobs/app, 2026-10-09)

- Choose and ready screens in Qt Quick, split into components from the prototype; the live Omarchy palette (full colors.toml + Hyprland rounding via Monologue's theme.cpp, reused by path), remembered pick, real display name. 18 unit tests pass (palette parsing, live theme switch, theme folder replaced, contrast guard, app state).
- Proven in the VM: one running process changed from tokyo-night to Catppuccin Latte when the real Omarchy theme was switched (docs/app/live-*.png). Found and fixed a real problem: Tokyo Night's own dim text colours measure 1.6 to 2.4:1; the palette now lifts dim text to 4.5:1 (rendered card text measured 4.65:1).
- Not wired, and labelled on screen: recording, hotkey, mic/camera, Pro drawer, full OBS, the OBS link.

## Open

- Wayland screen capture is PROVEN end to end on gus (ADR 0005, spike 3b): pre-seeded screen source, hidden OBS, one picker click, real frames, then a silent rerun with the saved restore token. Two cautions: a screen source created live over websocket never opens the picker (load it from a scene collection file instead), and a REGION selection recorded black frames (two runs) while another selection worked. Next to test: loading the collection with SetCurrentSceneCollection.
- New dependency: qt6-websockets (not on gus yet).
- Not built: the Alt+Shift+V hotkey (D-Bus), camera sources, the review screen, the Pro drawer and its tabs, a live preview, the recording pill, the first-run permission screen, a real run on gus with a screen/window pick.
- Throwaway test configs for gus runs live in the Claude scratchpad, not in ~/.config/obs-studio (Fred's own OBS setup was never touched).
