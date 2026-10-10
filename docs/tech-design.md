# MOBS tech design (draft 2, 2026-10-09)

Status: architecture approved by Fred 2026-10-09 (ADR 0005, which replaces the plugin-in-OBS plan of
ADR 0003). Draft 1 planned docks inside OBS's window; the spike and Fred's request for an interface
that does not look like OBS changed that. Inputs: README.md, docs/screen-design.md, ADRs 0002 to 0005.

## The answer in one paragraph

MOBS is **its own Omarchy-styled Qt Quick app** (`mobs/app/`) that starts OBS Studio hidden and drives
it over **obs-websocket**. The user only ever sees the MOBS window: a Simple view for beginners and a
Pro drawer for experts, with "Open full OBS" bringing up the real OBS window. OBS does all recording.
Monologue's editor, theme watcher and ffmpeg helpers are reused **by path** for the review screen.
Neither `obs/` nor `monologue/` is edited, so upstream pulls stay conflict-free (ADR 0002).

## Parts

```
mobs (one launcher, one window)
  ├─ MOBS app  (Qt Quick, normal OpenGL, mobs/app/)
  │    ├─ screens        choose · ready · recording · review · Pro drawer   (docs/screen-design.md)
  │    ├─ ObsLink        obs-websocket v5 client; start/stop/pause, scenes, sources, file path back
  │    ├─ ObsProcess     starts OBS hidden (Hyprland special workspace), enables websocket on
  │    │                 localhost with a generated password, quits it with MOBS
  │    ├─ Review         Monologue's edit.cpp, mediautils.cpp, thumbnails.cpp, EditBar.qml, theme.cpp
  │    │                 (included by path from ../monologue/src)
  │    └─ Control        D-Bus org.mobs.Control (Toggle, Pause, Status)
  └─ OBS Studio (obs/, untouched), running in the background

Hyprland bind  →  busctl --user call org.mobs.Control ... Toggle        (Alt+Shift+V)
```

Flow: hotkey or button → ObsLink `StartRecord` → OBS records → stop → OBS returns the output path →
Review opens it → trim and cut → Save (ffmpeg, atomic, original kept).

## What reading the code showed

**Monologue is small: about 3,600 lines of Qt 6 QML and C++17, and half of it duplicates OBS.**

| Part of Monologue | Lines | In MOBS |
|---|---|---|
| Camera and mic capture (`QMediaCaptureSession`, `audiocapture.cpp`, `writer.cpp`) | about 900 | **Dropped.** OBS records far better (v4l2, PipeWire, hardware encoders). |
| Editor: clips, split, trim, undo, filmstrip (`edit.cpp`, `EditBar.qml`, edit half of `backend.cpp`) | about 900 | **Kept: the review screen.** |
| ffmpeg probe, thumbnails, atomic save (`mediautils.cpp`, `thumbnails.cpp`) | about 250 | Kept. |
| Omarchy accent and corner sync (`theme.cpp`) | 82 | Kept, and extended to the full palette. |
| Portal save dialog (`portalfilepicker.cpp`) | 183 | Kept. |

`EditBar.qml` depends only on a `backend` object (`duration`, `clips`, `pauses`, `thumbnails`, and
`split`, `setClip`, `removeClip`, `joinClips`, `beginGesture`, `endGesture`). The prototype runs it
unmodified over a sample backend. Monologue's own `Backend` mixes capture and editing in one class,
so MOBS gets an editing-only backend; a later PR to omacom/monologue splitting it would let us drop the duplicate.

## Decisions

1. **Own window, OBS behind it** (ADR 0005). Reasons: it can look like Omarchy, not OBS; it uses Qt's normal
   OpenGL video output; and an OBS crash cannot take the MOBS window down. The spike showed QML docks inside
   OBS exit cleanly only about half the time on gus.
2. **Control over obs-websocket**, which is built into OBS 32 (`/usr/lib/obs-plugins/obs-websocket.so`).
3. **Hotkey over D-Bus on the MOBS app**, because OBS's own hotkeys do not fire globally on Wayland.
   **Start/stop: physical Alt + Shift + V** (Hyprland `SUPER + SHIFT + V`). **Pause: Ctrl + Alt + Shift + V.**
   Alt + Shift + R was Fred's first choice but is "Rename workspace" in `bindings.lua`. Both V chords were
   free in the live Hyprland table on 2026-10-09; re-check before binding.
4. **Pause marks from MOBS's own sidecar file**, written when it sends pause and resume, so they work for any format.
5. **Beginner cards are scenes MOBS creates over websocket**; the Pro drawer edits the same objects.
6. **Reuse Monologue by path, never copy**, so subtree pulls update the editor for free.

## Evidence so far

- **Spike 1 and 2** (`mobs/spike/`, ADR 0004): Monologue's real `EditBar.qml` ran inside an OBS dock with
  Qt Quick's software renderer; video needs `QVideoSink` to `QImage` (16 to 20 ms of CPU per frame at 640 wide).
  Exit reliability with any QML dock loaded was poor on gus (no plugin 6/6 clean; plugin without a dock 6/6;
  empty QML dock 4/8 stuck; full dock 3/6). One cause found (OBS's "Closing Dockable Window" dialog), the rest
  unexplained. This is why the architecture moved out of OBS's window.
- **Spike 3** (`mobs/spike3/`): hidden OBS driven over obs-websocket, all checks passed except the portal (see Risks).
- **Screen prototype** (`mobs/design/`, `docs/design/*.png`): five screens in the live Liftoff theme,
  rendered in the Test Drive VM with sample data.

## Risks and the next test

- **Portal screen capture while hidden: understood, one click from proven.** Hiding OBS does not matter. The picker is a
  layer-shell overlay centered on the focused monitor (invisible to `hyprctl clients`). gus's portal backend had wedged and
  needed a restart (ADR 0005). MOBS must close portal sessions cleanly, tell the user before the first picker appears,
  and rely on the restore token (`allow_token_by_default = true`) so later recordings are silent.
- **Profile must set `RecQuality`** (not `Stream`) or pause silently does nothing; **wait for `RecordStateChanged`
  STOPPED** before opening a take, because `StopRecord` replies before a Hybrid MP4 is finalized.
- **New dependency:** `qt6-websockets` (not installed on gus; installed in the VM for the spike).
- **First-run permissions:** the screen-capture portal prompt must be explained by a MOBS screen, not left to OBS.
- **Hiding OBS:** a Hyprland special workspace is Omarchy-only, which is fine here, but check that OBS stays alive and keeps capturing while hidden.
- **`obs-websocket` submodule is not checked out** in `obs/plugins/obs-websocket`. Harmless (we use the installed OBS), but a from-source OBS build needs `git submodule update --init`.
- **Windows and macOS** code stays in `obs/` untouched and unbuilt (README: left out).

## Next

1. Portal screen capture check on gus (one click), then the real app.
2. Wire the theme reader (`colors.toml`, live) and Hyprland rounding into the prototype.
3. Build `mobs/app/` screen by screen from the prototype.
