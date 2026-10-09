# 4. QML docks inside OBS use Qt Quick software rendering

- Date: 2026-10-09
- Status: decided (spike 1 evidence; Fred approved the plugin design in ADR 0003)

## Decision

The MOBS plugin calls `QQuickWindow::setGraphicsApi(QSGRendererInterface::Software)` before it
creates any `QQuickWidget`. Qt Quick draws the editor on the CPU, with no OpenGL context.

## Why

Spike 1 (`mobs/spike/`, run in Test Drive VM `test-drive-mobs-spike`, OBS 32.2.2, Qt 6.11.2,
native Wayland, EGL on Mesa virgl) tested Monologue's real, unmodified `EditBar.qml` in an OBS dock:

| Run | Qt Quick path | OBS preview (solid red scene) | Dock |
|---|---|---|---|
| Plugin off (control) | none | red | none |
| Plugin on, same config | OpenGL (default) | **black** | renders, animates |
| Plugin on | **software** | red | renders, animates, reacts to hover |

Same VM, same config, one variable changed each time. The default OpenGL path breaks OBS's own
preview; software rendering does not. A flat 2D timeline does not need the GPU.

## Consequences

- ADR 0003's fallback (editor in its own window) is not needed for the dock.
- Evidence: `docs/spike-1-dock-in-obs.png`; the animated block moved from x=345 to x=784
  between two screenshots with the preview pixel red in both.
- Not proven: real hardware. The VM uses a virtual GPU (virgl). Re-check on gus's hardware
  before calling it done, and re-test if the editor ever needs video playback inside the dock
  (`QtMultimedia` video output may want GL; the spike did not play video).
- `QQuickWindow::setGraphicsApi` is process-wide. OBS has no QML of its own, so this is
  safe today; re-check if OBS ever ships a Qt Quick component.
