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
- Portal screen capture: NOT yet proven end to end, but the blockers are understood (spike 3b, real gus hardware):
  - OBS hidden vs visible makes no difference; hiding is not the problem.
  - The xdph picker is a layer-shell overlay (`ch.wysbd.hyprland-preview-share-picker`, 1000x500, centered on the
    focused monitor), so it never shows in `hyprctl clients`; look in `hyprctl layers`. xdph shows it during `SelectSources`.
  - gus's xdg-desktop-portal-hyprland was wedged (100% CPU, no answers, running since Oct 6) until restarted with
    `systemctl --user restart xdg-desktop-portal-hyprland xdg-desktop-portal` (Fred approved). After the restart the
    picker appeared. An abandoned portal request is the likeliest way to wedge it; a clean `Session.Close` left it at 0.1% CPU.
  - Not yet seen: someone clicking the picker while hidden OBS captures (Fred was not at the desk for the run).
  - `~/.config/hypr/xdph.conf` has `allow_token_by_default = true`, so after one pick OBS keeps a restore token and later
    captures need no picker. MOBS's first run therefore needs one friendly "pick your screen" step; later runs are silent.