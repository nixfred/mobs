# 5. MOBS is its own Omarchy-styled window that drives OBS in the background

- Date: 2026-10-09
- Status: decided (Fred, 2026-10-09). Replaces the plugin-in-OBS part of ADR 0003.

## Decision

MOBS is its own Qt Quick app with its own window. It controls OBS Studio, running in the
background, over OBS's built-in obs-websocket (v5). The MOBS window does not look like OBS:
it follows the live Omarchy theme, with a Simple view for beginners and a Pro drawer for
experts. "Open full OBS" brings up the real OBS window for anything the Pro drawer lacks.

- `obs/` stays untouched and is built or installed as-is. `monologue/` stays untouched.
- MOBS's code lives in `mobs/app/` and reuses Monologue's editor, theme watcher, thumbnails
  and ffmpeg helpers by path, not by copy.
- The global hotkey (Alt + Shift + V) is a D-Bus method on the MOBS app, called from a
  Hyprland bind. OBS's own hotkeys do not fire globally on Wayland.

## Why

Fred, 2026-10-09: the interface must be super easy for beginners and still satisfy experts,
and it looked "TOO much like OBS" with Omarchy feel wanted. A dock inside OBS's window can
never stop looking like OBS. The spike also showed the embedded route is unreliable on gus:
with the QML dock loaded OBS exited cleanly only about half the time (no plugin: 6 of 6;
plugin loaded without a dock: 6 of 6; empty QML dock: 4 of 8 stuck; full dock: 3 of 6).
One cause was found (OBS's one-time "Closing Dockable Window" dialog, seen on the main
thread in `QDialog::exec`), but pre-setting its flag did not make every run clean, so the
rest is unexplained. A separate process avoids all of it and also lets MOBS use Qt's normal
OpenGL video output instead of the software-rendered workaround of ADR 0004.

## Consequences

- ADR 0004 (software rendering for QML docks) only applies if a dock inside OBS is ever built again.
- Two processes, one app to the user: MOBS starts OBS hidden (a Hyprland special workspace),
  enables obs-websocket on localhost with a generated password, and quits it with MOBS.
- Spike 3 (mobs/spike3/, practice VM, OBS 32.2.2, obs-websocket 5.7.4) PROVED the control plane: OBS started
  with a generated websocket password, hidden on Hyprland's `special:mobs` (verified with `hyprctl clients -j`),
  and a Qt WebSockets client authenticated, created a scene and input, started, paused (4.93 s file from
  3 s + 2 s recorded with 2 s paused), resumed and stopped, and got the file path; OBS stayed alive and hidden.
  Two things the first run got wrong, and what MOBS must do:
  - Pause only works when recording does not share the stream encoder: simple-mode `RecQuality` must not be
    `Stream` (OBS's default). MOBS's profile sets `RecQuality=Small`.
  - `StopRecord` returns the path BEFORE a Hybrid MP4 is finalized (the file looked 0 bytes). Wait for the
    `RecordStateChanged` event with state STOPPED (about 0.7 s later) before opening the take.
- Portal screen capture (spike 3b, real gus hardware). Not yet seen with a human click, but the cause of every
  earlier failure is now known and it changes how MOBS must build the screen source:
  - **A PipeWire screen source created through obs-websocket `CreateInput` never opens the picker.** A D-Bus capture
    shows OBS sends `CreateSession`, the portal replies, and OBS never sends `SelectSources`; OBS's log never says
    "Screencast session created". Four runs, same result. (The one picker I saw earlier was a leftover of my own direct test.)
  - **The same source created at OBS load (pre-seeded in the scene collection JSON) works:** OBS logged "Screencast
    session created", sent `SelectSources` 0.08 s after `CreateSession`, and the picker stayed up until answered.
    Likely mechanism (inferred, not proven): OBS's portal reply is handled on the thread that created the source;
    the UI and scene-load paths run on the main loop, the websocket worker thread has none.
  - **End to end now proven on real gus hardware** (spike 3b, last runs): pre-seeded source, hidden OBS, one click on the
    picker, OBS saved a RestoreToken into the scene file, hidden OBS recorded real frames (2,081 distinct colours in a
    recorded frame; a screenshot payload of 251,666 characters, versus 534 for a black frame). A rerun with the saved
    token needed NO picker and recorded again silently (4.7 s file, STOPPED event).
  - **Region capture gave black frames** (two runs, one hidden, one visible, a 1681x1203 region; the stream said `streaming`
    but every pixel was 0). A different selection (470x1005) gave real frames. gus's layout has negative coordinates
    (DP-3 at -2560,-2160), a plausible but unproven cause. Until investigated, MOBS offers screen and window only, and checks
    the first frame is not black before telling the user it is recording.
  - So MOBS must put the screen source in a scene collection file and load it (OBS reads it on its main thread), not
    create it live. Switching collections (`SetCurrentSceneCollection`) is the thing to test next.
  - Hiding OBS makes no difference. The picker is a layer-shell overlay (`ch.wysbd.hyprland-preview-share-picker`,
    1000x500, centered on the focused monitor), invisible to `hyprctl clients`; look in `hyprctl layers`.
  - gus's xdg-desktop-portal-hyprland had wedged (100% CPU, no answers) and was restarted with Fred's OK:
    `systemctl --user restart xdg-desktop-portal-hyprland xdg-desktop-portal`. Abandoned portal requests are the likely
    cause; kill nothing mid-picker. An orphaned picker left by a killed client stays on screen until its process is closed.
  - `~/.config/hypr/xdph.conf` has `allow_token_by_default = true`: after one pick OBS keeps a restore token, so later
    captures need no picker. MOBS's first run needs one friendly "pick your screen" step; later runs are silent.
