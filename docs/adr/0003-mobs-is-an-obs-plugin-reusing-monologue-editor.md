# 3. MOBS is an OBS plugin that reuses Monologue's editor

- Date: 2026-10-09
- Status: proposed (Larry's draft; Fred has not yet reviewed it)

## Decision

MOBS's own code is an out-of-tree OBS plugin in `mobs/`. It adds a record dock and an edit
dock to OBS, reuses Monologue's editor source by path (no copies), and takes a global
hotkey over D-Bus. OBS does all recording. Monologue's camera capture is not used.
Full reasoning and the part-by-part table are in `docs/tech-design.md`.

## Why

Keeps `obs/` and `monologue/` untouched, so the subtree pulls in ADR 0002 stay conflict-free,
while still giving one window and one process (Fred, 2026-10-09: "one combined app from day one").

## Considered

- **Patch Monologue's recorder into OBS itself:** duplicates what OBS already does better and
  puts constant merge conflicts inside `obs/`.
- **Two programs glued by a script:** simplest, but Fred chose one combined app.
- **Use Monologue's `Backend` as is:** drags Qt Multimedia camera capture into the OBS process.

## Consequences

- Fallback if QML docks fail inside OBS on Wayland: the editor opens in its own window from
  the same plugin. To be settled by the spike in a Test Drive VM before any real plugin code.
- Replace this note with a new one if the spike changes the plan; do not rewrite it.
