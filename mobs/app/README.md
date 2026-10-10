# MOBS app (skeleton)

MOBS's own window (ADR 0005). Qt 6 Quick, GPLv2. It reuses Monologue's `theme.cpp` **by path** from
`../../monologue/src`, so subtree pulls keep updating it.

**What works now:** the choose and ready screens; the live Omarchy theme (full `colors.toml` palette plus Hyprland
rounding), including theme switches and a contrast guard for dim text; remembering your pick between launches;
the real display name and size. **Not wired yet (and labelled so on screen):** recording, the hotkey, mic and camera,
the Pro drawer, full OBS, the OBS link.

```sh
cmake -S . -B build -G Ninja && cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure   # 18 tests, no window appears
./build/mobs
```

Test and screenshot switches (env vars): `MOBS_THEME_DIR` (a fake Omarchy "current" folder), `MOBS_SETTINGS` (settings file),
`MOBS_SCREEN=1` / `MOBS_PICK=1..3` (start screen), `MOBS_GRAB=/path.png` (save the window's own pixels and quit).

Layout: `src/palette.*` (theme reader), `src/appstate.*` (screens and pick), `qml/` (components and screens), `tests/`.
Screens are checked in the Test Drive practice VM, never on a live desktop; see `docs/app/`.
