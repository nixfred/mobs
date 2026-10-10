# MOBS screen design (draft 1, 2026-10-09)

> Update 2026-10-10: the "full obs" door in the header is gone (ADR 0006: there is no separate OBS app; the Pro drawer must grow to cover it). The ready screen now also shows a red warning under a recording that came out black.

Status: approved direction (Fred, 2026-10-09). The pictures are real renders of a prototype
(`mobs/design/`, built and run in the Test Drive practice VM) using **sample data only**: it
records nothing and talks to nothing. They show the design, not a working recorder.

## What Fred asked for

"Super easy for beginners but also satisfy experts. It looks too much like OBS right now.
Put some Omarchy feel on it."

Three answers settled the shape:

- MOBS is **its own window**; OBS runs behind it (ADR 0005).
- A beginner sees **three big cards, then one Record button**.
- Experts get a **Pro drawer** in the same look, plus **Open full OBS** for everything else.

## Principles

1. **One obvious thing per screen.** A beginner never sees the words scene, source, bitrate or filter.
2. **Pro is one key away, never in the way.** `P` slides the drawer in; it never changes what the main area does.
3. **Keyboard first, mouse fine.** Every screen shows its keys as keycap chips in the footer.
4. **Looks like Omarchy, not like OBS.** Same palette, font, corner rounding and window border as the desktop around it.
5. **Nothing is lost.** All of OBS stays reachable: Pro for the common settings, Open full OBS for the rest.

## Omarchy feel

- **Palette:** read live from the active Omarchy theme (`~/.local/state/omarchy/current/theme/colors.toml`,
  already how Monologue's `theme.cpp` follows it). The prototype uses the theme on gus today, **Liftoff**:
  background `#151E26`, raised surfaces `#253440`, borders `#334D5D`, text `#ECE5D8`, muted `#88979F`,
  accent `#F3B77B`, record red `#EF8D80`, level green `#A9C59A`. Switch themes and MOBS follows.
- **Type:** JetBrainsMono Nerd Font (the Omarchy font), lowercase and terse. Icons are Nerd Font glyphs, so they match the terminal.
- **Shape:** corner radius follows Hyprland's live `decoration:rounding` (24 on gus); Hyprland draws the window border, so MOBS draws none.
- **Keys:** shortcuts are keycap chips, written with physical key labels (Alt is your SUPER key on gus).
- **Quiet:** no menu bar, no toolbar, no tabs on the main screens.

## The screens

| # | Screen | Who it's for | Picture |
|---|---|---|---|
| 1 | Choose: "what are we making?" | first run, and "new" | `design/screen-1.png` |
| 2 | Ready: one big record button | everyone | `design/screen-2.png` |
| 3 | Recording: big timer, pause, stop | everyone | `design/screen-3.png` |
| 4 | Review: preview, Monologue's timeline, save | everyone | `design/screen-4.png` |
| 5 | Ready + Pro drawer | experts | `design/screen-5.png` |

`design/in-omarchy-vm.png` shows screen 1 inside the Omarchy desktop in the VM (the toasts in the corner are the VM's own tips).

**Flow:** choose (1) → ready (2) → record (3) → review (4) → save → back to ready (2).
`Alt + Shift + V` from anywhere jumps ready (2) → recording (3), and again → review (4).
`P` toggles the Pro drawer on ready (5).

### 1. Choose
Three cards: **my screen**, **screen + me**, **just my camera**, keys `1 2 3`. MOBS remembers the pick, so
later launches open on screen 2 with that pick; "new" brings this screen back.

### 2. Ready
A big record button (accent ring, red dot), the plain-language state ("my screen, with mic"), the hotkey
chips, three small chips that show what will be recorded (display, mic, camera), a live mic level, and the
last three takes. Nothing here needs explaining.

### 3. Recording
A pulsing red dot, a large timer, pause and stop buttons, and the two hotkeys. The window can shrink to a
small always-on-top pill while recording so it never sits in your shot (to be designed with the plugin).

### 4. Review
The take opens right after you stop. Preview on top, **Monologue's real timeline editor** underneath (split,
trim, cut ranges, pause marks, undo; its source file is used unmodified), then **save**, **save a copy**, **discard**.
Edits never touch the original (Monologue's rule, kept).

### 5. Pro drawer
Tabs: **sources** (display, mic, camera, layout), **audio** (levels, per-source gain, noise tools), **output**
(format, encoder, quality, frame rate, folder), **keys** (hotkeys). **Open full OBS** sits at the bottom with
"everything else is there." Built 2026-10-10 (see below): recording, audio, keys and about tabs; sources, encoder and frame rate are still to come.

## How the beginner choices map to OBS

The three cards become scenes MOBS creates in OBS over obs-websocket (display capture, camera, mic), so a
beginner never touches a scene or source. The Pro drawer edits those same objects. In the real build the
recorded file's path comes back over the same link, and the review screen opens it.

## What is real and what is sample

- **Real:** the layout, palette, type, spacing, key hints, and Monologue's `EditBar.qml` in screen 4 (unmodified file, recoloured by the theme accent).
- **Sample:** every value on screen (monitor, mic level, timer, take list, thumbnails, durations), and the preview frame.
- **Not built:** theme reading, OBS control, the hotkey, the recording pill, the audio/output/keys tabs.

## Open items

- Screen 3's small pill mode, and where the Pro drawer's audio/output/keys tabs get their controls.
- First-run permission prompts (Wayland screen capture goes through a portal) need a friendly screen of their own.
- Whether "save a copy" belongs on the main row or behind a menu.
- A real-hardware look on gus (this was rendered in the VM).

## Camera panel (added 2026-10-10)

The camera chip on the ready screen is a button. It opens a panel (screenshots: `docs/app/camera-b.png`, `camera-c.png`,
`camera-e.png`, `camera-d.png`) with: what we're making (my screen, screen + me, just my camera); which camera (the
webcams found on this machine, by name); and for *screen + me* a mini screen where you tap a corner, plus small, medium
or large. The mini screen shows exactly where the camera will be in the recording (same rectangle). *Just my camera* fills
the frame, so it needs no placement. Choosing a camera mode with no camera chosen opens this panel by itself.

## Review screen (built 2026-10-10)

Screens: `docs/app/review-r1.png` (just stopped), `review-r2.png` (after cutting the middle out), `review-r3.png` (after saving). Video on top,
Monologue's timeline underneath (kept clips lit, cut parts dimmed, white marks where you paused), a play button with the time, then
**save edit** (filled), **done**, and **delete take** (red, asks first). Leaving with unsaved edits asks first ("leave without saving").

## Pro drawer (built 2026-10-10)

Screenshot: `docs/app/pro-drawer.png`. A 420 px drawer slides in on the right; the screen beside it re-centres in the space that is left. Tabs: **recording** (quality small / balanced / high with a plain-language line under it, save-recordings-in folder field that validates as you leave it, open folder, reset, "open the editor after recording", "live preview"), **audio** (record my microphone), **keys** (Alt Shift V, Ctrl Alt Shift V, Space, P, Esc, and the editor keys) and **about**. Opened by the lit-up "pro" chip or P; closed by the chip, P or Esc. P is ignored while recording, so nothing changes under a running take.
