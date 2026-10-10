# 6. MOBS is a product built on libobs, not an overlay on the OBS app

- Date: 2026-10-09
- Status: decided (Fred, 2026-10-09: "I don't want an overlay. I want a new product. I was clear about that.").
  Replaces ADR 0005 (own window driving a hidden OBS over websocket) and, with it, ADR 0003.

## Decision

MOBS is **one application**: its own Omarchy-styled Qt Quick frontend with **libobs embedded in the same process**,
the OBS engine plugins it needs (capture, encoders, outputs, filters), and Monologue's editor. There is no OBS
application, no websocket, and nothing hidden. This is how OBS Studio itself is built (a frontend on libobs); MOBS
is a different frontend on the same engine, with Monologue's editing joined in.

- `obs/` stays an untouched subtree with its history. OBS already has a build switch, `ENABLE_FRONTEND=OFF`,
  that builds libobs and the plugins without OBS's UI. MOBS builds and ships that engine with its own binary.
- MOBS's code lives in `mobs/app/` and links libobs. It never edits `obs/` or `monologue/` (ADR 0002 stands).
- "All of OBS's features" is reached by building MOBS's own UI over libobs's API, using libobs's property system to
  generate controls for any source, filter or encoder, so new OBS features arrive with upstream pulls.
- The hotkey is a D-Bus method on MOBS itself (Alt + Shift + V), as before.

## Why

Fred chose, at the start, "a new project… ALL the features of OBS and Monologue, made into one big usable project"
and rejected the wrapper option. ADR 0005 drifted back into a wrapper: a skin over a separate OBS program. A product
is one program he owns end to end: no second process to start, hide, authenticate to or lose.

## Evidence (spike 4, `mobs/spike4/`, Test Drive VM, libobs 32.2.2, native Wayland)

A Qt program with libobs embedded recorded a valid file with no OBS application running: OpenGL video through
`libobs-opengl` on Wayland, 12 of 13 engine plugins (NVENC absent in the VM, as expected), x264 + AAC, `mp4_output`
with pause and resume (4.8 s file from 3 s + 2 s paused + 2 s), exit code 0 in 16 of 16 runs once three things were right:

1. Set the Wayland platform and display **before** `obs_startup` (OBS's own `OBSApp::OBSInit` does).
2. Load only **engine** plugins by name. `obs_load_all_modules()` also loads OBS's UI plugins (websocket, frontend
   tools, decklink UI), which call a frontend that does not exist here, and crashes.
3. Call `obs_wait_for_destroy_queue()` before `obs_shutdown()`, and remove scene items explicitly; otherwise libobs's
   graphics thread releases a source during shutdown (crash, then a "Double destroy" warning).

## Consequences

- Gone: obs-websocket, the hidden OBS window, `qt6-websockets`, spike 3's control plane, the scene-file loading
  question, and the "dock exits unreliably" problem. Spike 3's portal findings carry over: the restore token, the
  picker overlay, and region captures that gave black frames.
- Proven next, on real gus hardware (spike 4b): **PipeWire screen capture works in-process.** The source is created on the
  Qt main thread (which iterates the GLib loop), so the portal reply is handled; run 1 opened the picker, Fred picked, the source
  was 470x1005 after 12.8 s, and the recording (with a pause) had real content (1,871 distinct colours), exit 0. MOBS then saved
  the restore token itself (`obs_source_save`, `RestoreToken`) and run 2 needed **no picker**: source ready in 250 ms, real
  content again (2,279 colours). The picker overlay was gone afterwards. libobs printed "Cannot create EGLImage" twice
  (GPU-buffer import falling back; frames were fine), to watch on the hybrid Intel/NVIDIA machine.
- Still to prove: a live preview inside the Qt Quick scene, and the first-frame-not-black check as code (region picks gave black).
- MOBS owns more: encoder and output settings, a source list, audio mixing, scene handling, and persistence of its
  own settings. The prototype's screens and the live Omarchy palette are reused as they are.
- Packaging: MOBS ships its own libobs build (GPLv2, like OBS). Development uses the system libobs 32.2.2 until
  the subtree build is wired up.

## First real run of the MOBS app on gus (2026-10-10, 00:0x)

Driven by the app's own test script (`MOBS_SCRIPT`, no synthetic key presses), real libobs, real PipeWire capture:
exit code 0 in both runs; run 1 opened the picker once and saved the token, run 2 needed no picker; the files were
5.20 s and 5.00 s (3 s + 2 s recorded, the 2 s pause cut out), 1920x1080 H.264 + AAC. **The video was black**, because
Fred picked a **region** (confirmed): that makes three independent runs (OBS-driven and MOBS) where a region pick records
black while a screen or window pick records real frames. MOBS now inspects every saved recording (a still from 1 s in,
32x18 grey, via ffmpeg) and shows a red warning that names the likely cause; the picker prompt tells people to choose a
screen or window. Found by a test while writing it: QByteArray chars are signed, so 255 read as -1 and a mixed
black-and-bright frame could have been called black.
