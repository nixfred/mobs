# MOBS tech design (draft 1, 2026-10-09)

Status: approved by Fred 2026-10-09 (ADR 0003). Written from reading both codebases. Nothing here is built yet.
Inputs: README.md (what was agreed), docs/adr/0002 (one repo, two upstreams).

## The answer in one paragraph

MOBS is **OBS Studio plus one plugin that lives outside `obs/`**. The plugin adds two
docks to OBS's window, a record/pause strip and Monologue's trim editor, and answers a
global hotkey. OBS keeps doing the recording; Monologue's editor does the cutting. The
result is one window, one process, and neither `obs/` nor `monologue/` has to be edited,
so upstream pulls stay conflict-free (ADR 0002).

## What reading the code showed

**Monologue is small: about 3,600 lines of Qt 6 QML and C++17, and half of it duplicates OBS.**

| Part of Monologue | Lines | In MOBS |
|---|---|---|
| Camera and mic capture (`QMediaCaptureSession`, `audiocapture.cpp`, `writer.cpp`) | about 900 | **Dropped.** OBS records far better (v4l2, PipeWire, hardware encoders). |
| Editor: clips, split, trim, undo, filmstrip (`edit.cpp`, `EditBar.qml`, edit half of `backend.cpp`) | about 900 | **Kept. This is the part OBS lacks.** |
| ffmpeg probe, thumbnails, atomic save (`mediautils.cpp`, `thumbnails.cpp`) | about 250 | Kept. |
| Omarchy accent and corner sync (`theme.cpp`) | 82 | Kept. |
| Portal save dialog (`portalfilepicker.cpp`) | 183 | Kept. |

**OBS gives a plugin everything needed, through `obs-frontend-api.h` (installed: OBS 32.2.2, headers in `/usr/include/obs`):**

- `obs_frontend_add_dock_by_id(id, title, QWidget*)` puts a panel in OBS's window.
- `obs_frontend_recording_start/stop/pause`, plus `OBS_FRONTEND_EVENT_RECORDING_STARTED/STOPPED/PAUSED/UNPAUSED` events.
- `obs_frontend_get_last_recording()` says where the finished file is, so the editor can open it on stop.
- `obs_frontend_recording_add_chapter()` exists too.

**The tools line up:** OBS is Qt 6 widgets, Monologue is Qt 6 QML. Arch ships one Qt
(6.11.2) with `QQuickWidget` in `qt6-declarative`, so a QML editor can sit inside an OBS
dock in the same process.

## Parts

```
OBS Studio (obs/, untouched)
  └─ loads plugin  mobs/  (new, ours, GPLv2)
       ├─ RecordDock    big record button, pause, timer, mic meter, hotkey hint
       ├─ EditDock      QQuickWidget showing Monologue's editor QML
       ├─ EditorBackend edit + save logic, built from monologue/src/edit.cpp,
       │                mediautils.cpp, thumbnails.cpp, theme.cpp (included by path, not copied)
       ├─ PauseLog      writes <take>.pauses.json from pause/resume events
       └─ Control       D-Bus service  org.mobs.Control  (Toggle, Pause, Status)

Hyprland bind  →  busctl --user call org.mobs.Control ... Toggle
```

Flow: hotkey (or button) → OBS records → stop event → file path from
`obs_frontend_get_last_recording()` → EditDock opens the take with pause marks from the
sidecar → trim and cut → Save (ffmpeg, atomic, original kept).

## Decisions made in this draft

1. **Plugin, not patching OBS.** MOBS code goes in `mobs/`, built against the installed OBS
   headers. Only a very small, rebased change may ever touch `obs/` or `monologue/`.
2. **Reuse Monologue's files by path, do not copy them.** `mobs/CMakeLists.txt` points at
   `../monologue/src/*.cpp` and the QML files, so a subtree pull updates the editor for free.
3. **A new `EditorBackend`, not Monologue's `Backend`.** Monologue's `Backend` mixes capture
   and editing in one 550-line class, and the capture half would drag Qt Multimedia's
   camera into OBS. MOBS gets an editing-only backend and `monologue/` stays pristine.
   Later, a PR to omacom/monologue splitting its `Backend` the same way would let us drop
   the duplicate (we hold clones in `forks/` for exactly that).
4. **Hotkey over D-Bus, not OBS hotkeys and not obs-websocket.** OBS's own hotkeys do not
   fire globally on Wayland. A Hyprland `bind` running `busctl --user call` needs no extra
   program and no websocket password, and Monologue already links QtDBus. The README
   still says "global hotkey plus buttons", and this is how the hotkey works.
5. **Pause marks come from our own sidecar file,** written when the pause and resume
   events fire, not from OBS chapter markers, because it works for any container format.
6. **Camera and mic are OBS sources.** MOBS ships a default scene collection ("MOBS": screen,
   camera, mic) and a recording profile (H.264 MP4 and AAC, like Monologue).

## Risks and the first test

- **Was the biggest unknown: a `QQuickWidget` inside an OBS dock on Wayland. Spike 1 answered it (ADR 0004).**
  The default OpenGL path turned OBS's preview black; forcing Qt Quick's software renderer fixed it,
  and Monologue's real `EditBar.qml` ran in the dock. Not yet proven on real hardware (the VM is virgl),
  and video playback inside the dock was not tested.
- **Qt Multimedia FFmpeg backend inside OBS:** only matters if the editor plays video through
  `QtMultimedia`. Both link the system ffmpeg on Arch, but check for symbol clashes in the spike.
- **`obs-websocket` submodule is not checked out** in `obs/plugins/obs-websocket`. Harmless
  here (we build against installed OBS), but a from-source OBS build would need
  `git submodule update --init`.
- **Windows and macOS** code stays in `obs/` untouched and unbuilt (README: left out).

## Hotkeys (decided by Fred, 2026-10-09)

- **Start/stop:** physical Alt + Shift + V (Hyprland `SUPER + SHIFT + V`). Pause/resume: physical Ctrl + Alt + Shift + V (`SUPER + CTRL + SHIFT + V`).
- Alt + Shift + R was Fred's first choice but is taken by "Rename workspace" (`bindings.lua`), so V (video) won. Both V chords were free in the live Hyprland table on 2026-10-09; re-check before binding.
- Described by physical key labels: Alt sends SUPER on gus, no Windows-logo key needed.

## Not decided here

- Screen design (layout of the two docks, the first-run flow). Separate step on the Medium track.
- Whether the default scene collection ships in the plugin or is created on first run.
- Naming of the D-Bus interface beyond `org.mobs.Control`.

## Next

1. Practice copy: done (Test Drive VM `test-drive-mobs-spike`).
2. Dock spike: done, passed with software rendering (ADR 0004, `docs/spike-1-dock-in-obs.png`).
3. Spike 2: play a recorded take inside the dock (video output without GL), and re-check on real hardware.
4. Screen design, then the plugin.
