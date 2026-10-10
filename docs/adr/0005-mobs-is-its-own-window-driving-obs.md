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
- Still to prove (spike 3): MOBS can start OBS hidden, connect over websocket, start, pause and
  stop a recording, and get the file path back; and Wayland screen capture via the portal works
  when OBS is not the focused window.
- Beginner presets ("Record my screen", "Screen + my camera", "Just my camera") become scenes
  MOBS creates over websocket, so beginners never see scenes or sources.
