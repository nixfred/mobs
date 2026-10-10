# MOBS tech design (draft 3, 2026-10-09)

Status: architecture decided by Fred 2026-10-09 (ADR 0006). Drafts 1 and 2 planned docks inside OBS, then a window
driving a hidden OBS over websocket. Fred rejected both as overlays: "I don't want an overlay. I want a new product."
Inputs: README.md, docs/screen-design.md, ADRs 0002, 0004, 0005 (findings only) and 0006.

## The answer in one paragraph

MOBS is **one application**. Its own Omarchy-styled Qt Quick frontend has **libobs embedded in the same process**
(the OBS engine: capture, encoding, outputs), loads the OBS engine plugins it needs, and joins Monologue's editor for
reviewing takes. There is no OBS application, no websocket and nothing hidden. This is the architecture of OBS Studio
itself (frontend + libobs); MOBS is a different frontend on that engine. `obs/` and `monologue/` stay untouched
subtrees that keep pulling from their official upstreams (ADR 0002).

## Parts

```
mobs  (one binary, one window, one process)
  ├─ Frontend (Qt Quick, mobs/app/)
  │    ├─ screens        choose · ready · recording · review · Pro drawer     (docs/screen-design.md)
  │    ├─ Palette        live Omarchy theme (colors.toml) + Hyprland rounding + contrast guard   [built]
  │    ├─ AppState       screens, remembered pick                                                [built]
  │    ├─ Engine         state machine over a RecorderBackend; ObsBackend wraps libobs (swappable)   [built]
  │    ├─ Review         Monologue's edit.cpp, mediautils.cpp, thumbnails.cpp, EditBar.qml (by path)
  │    └─ Control        D-Bus org.mobs.Control (Toggle, Pause, Status) for the Alt+Shift+V bind
  ├─ libobs + engine plugins   built from the obs/ subtree with ENABLE_FRONTEND=OFF
  └─ Monologue editor source   monologue/ subtree, used by path
```

Flow: Record (key or button) → Engine starts the output → stop → file path → Review opens it → trim and cut → Save
(ffmpeg, atomic, original kept).

## Engine rules (all proven in spike 4, `mobs/spike4/libobs-record.cpp`)

1. Set the Wayland platform and display (`obs_set_nix_platform(WAYLAND)`, `obs_set_nix_platform_display`) **before** `obs_startup`.
2. Then `obs_startup`, data and module paths, `obs_reset_video` (graphics module `libobs-opengl`), `obs_reset_audio2`.
3. Load **engine plugins by name**, never `obs_load_all_modules()`: that also loads OBS's UI plugins (websocket,
   frontend tools, decklink UI), which call a frontend that does not exist and crash. Used so far: obs-x264,
   obs-ffmpeg, obs-outputs, obs-filters, obs-transitions, image-source, text-freetype2, linux-pipewire,
   linux-pulseaudio, linux-v4l2, linux-capture, obs-nvenc, obs-qsv11 (NVENC only loads where an NVIDIA driver exists).
4. Record with `mp4_output`, x264 + `ffmpeg_aac`; pause and resume with `obs_output_pause`.
5. Shut down in order: release output, encoders, sources; detach scene items (`obs_sceneitem_remove`);
   `obs_wait_for_destroy_queue()`; then `obs_shutdown()`. Skipping the wait crashes libobs's graphics thread.

## What Monologue contributes

Monologue is about 3,600 lines of Qt 6 QML and C++17; half of it (camera, mic, writer) duplicates what libobs does
better. MOBS keeps the editor (`edit.cpp`, `EditBar.qml`, the edit half of `backend.cpp`), the ffmpeg probe and
thumbnails (`mediautils.cpp`, `thumbnails.cpp`), the theme watcher (`theme.cpp`) and the portal save dialog
(`portalfilepicker.cpp`). `EditBar.qml` needs only a `backend` object (`duration`, `clips`, `pauses`, `thumbnails`,
`split`, `setClip`, `removeClip`, `joinClips`, `beginGesture`, `endGesture`) and runs unmodified in the prototype.

## Decisions

1. **A product, not an overlay** (ADR 0006).
2. **libobs in-process**, engine plugins by name, built from the `obs/` subtree with `ENABLE_FRONTEND=OFF`
   (development uses the system libobs 32.2.2 until that build is wired up).
3. **All of OBS's features through libobs's API**, with generated controls from `obs_properties`, so features
   arrive with upstream pulls instead of being re-implemented by hand.
4. **Hotkey over D-Bus on MOBS itself**: physical Alt + Shift + V start/stop, Ctrl + Alt + Shift + V pause
   (`SUPER + SHIFT + V`, `SUPER + CTRL + SHIFT + V`; Alt + Shift + R is "Rename workspace"). Re-check before binding.
5. **Pause marks from MOBS's own sidecar file**, written when it pauses and resumes.
6. **The interface** is the screen design: three cards then one Record button; a Pro drawer; no "full OBS" window to open.
   (The prototype's "full obs" door goes: there is no OBS app any more, so the Pro drawer must grow to cover it.)

## Evidence so far

- **Spike 4** (`mobs/spike4/`): libobs embedded in a Qt program recorded with pause on native Wayland, 16 of 16 clean
  exits after the three fixes above. Details in ADR 0006.
- **App skeleton** (`mobs/app/`): choose and ready screens, the live palette with a contrast guard (Tokyo Night's own
  dim text is about 2:1), 18 tests, and a running process followed a live theme switch (`docs/app/`).
- **Portal findings from spike 3** (still true): the screen picker is a layer overlay centered on the focused monitor; a
  restore token makes later captures silent (`allow_token_by_default = true` on gus); a region selection recorded
  black frames in two runs. gus's portal can wedge if a client abandons a request; close sessions cleanly.
- **Spike 1 and 2** (ADR 0004): Qt Quick inside OBS's own window was unreliable, which no longer matters.

## Risks and next tests

- **Screen capture in-process: proven on gus** (ADR 0006, spike 4b): picker once, the token saved with `obs_source_save`
  makes later runs silent. Still to do: the first-frame-not-black check as code, and the "Cannot create EGLImage" warnings
  (GPU-buffer import fallback) on this hybrid-GPU machine.
- **Source and encoder breadth:** mic and camera sources, audio mixing, NVENC/VAAPI choices, and recording-quality settings.
- **Own the settings:** libobs has no profile or scene-collection files here; MOBS must save and restore its own.
- **Preview: DONE (2026-10-10)** without a native display surface: libobs raw video callback to QImage to a QML image provider (`mobs/app/src/previewmodel.*`, `obsbackend.cpp`). The risky GL-subsurface route was not needed.
- **Build:** wire `ENABLE_FRONTEND=OFF` from the subtree, and note `obs-websocket` is an uninitialized submodule
  (not needed any more).
- **Windows and macOS** code stays in `obs/` untouched and unbuilt (README: left out).

## Next

1. DONE in the VM (2026-10-09): `Engine` + `RecorderBackend` + `ObsBackend`, tests, and a record button that really records. Next: one real run on gus (picker once).
1b. The Alt+Shift+V hotkey over D-Bus.
2. DONE: live preview (see Risks).
3. The review screen with Monologue's editor; then the Pro drawer.
