# mobs: status

- Started: 2026-10-09, from Clarity's Idea Box
- Stage: shape, tech design, screen design and spike 3 done 2026-10-09. Next is the portal check on gus, then the build.
- Repo: github.com/nixfred/mobs (private). OBS and Monologue are git subtrees (obs/, monologue/) with full history and the official upstreams as remotes (ADR 0002).
- Finding: Monologue is a webcam recorder with a trim editor (Qt 6 QML, C++), not a voice tool. Voxtype (already on Omarchy) is the dictation tool.

## Decided (Fred, 2026-10-09; details in README.md)

- For: Fred only, on his Omarchy machines (gus, vic).
- Destination: all of OBS's and Monologue's features in one app. Left out: Windows and macOS, voice dictation and transcripts. Not ruled out: live streaming, packaging.
- Hotkey: physical Alt + Shift + V starts/stops (pause: Ctrl + Alt + Shift + V). Alt + Shift + R was taken (Rename workspace).
- Interface: "super easy for beginners, satisfy experts, not too much like OBS, Omarchy feel." MOBS is its own window in the live Omarchy theme; OBS runs behind it, driven over obs-websocket (ADR 0005, replaces ADR 0003). Beginner: three cards then one Record button. Expert: a Pro drawer plus "Open full OBS".

## Evidence

- Spike 1 and 2 (ADR 0004, mobs/spike/): Monologue's real EditBar.qml runs in an OBS dock on native Wayland with Qt Quick software rendering (OpenGL turned OBS's preview black, A/B in the VM); real video plays via QVideoSink to QImage, also on gus's real hardware. But OBS exits cleanly only about half the time with any QML dock loaded on gus (no plugin 6/6, plugin without dock 6/6, empty QML dock 4/8 stuck, full dock 3/6; one cause found, the "Closing Dockable Window" dialog, the rest unexplained). That is why MOBS left OBS's window.
- Screen prototype (mobs/design/, docs/design/screen-1..5.png, docs/screen-design.md): five screens in the live Liftoff theme, rendered in the Test Drive VM with sample data only. Screen 4 uses Monologue's EditBar.qml unmodified.
- Practice copy: Test Drive VM test-drive-mobs-spike (OBS 32.2.2, Qt 6.11.2, cmake/ninja; its package DB was stale, refreshed with pacman -Sy). Still running.

- Spike 3 (mobs/spike3/, ADR 0005): PASSED for the control plane in the VM. OBS hidden on special:mobs, websocket auth, scene/input, record, pause (cut 2 s of a 7 s run), resume, stop, file path back, OBS alive throughout. Learned: RecQuality must not be Stream or pause does nothing; wait for the RecordStateChanged STOPPED event, the file is not final when StopRecord replies.

## Open

- Wayland screen capture through the portal: blockers understood (ADR 0005, spike 3b). The picker is a 1000x500 layer-shell overlay on the focused monitor; gus's portal had wedged and was restarted. Still needs one run with Fred at the desk to click the picker.
- New dependency: qt6-websockets (not on gus yet).
- Not built: theme reader (colors.toml, live), the real app, the hotkey, the recording pill, the audio/output/keys tabs, first-run permission screen.
- Throwaway test configs for gus runs live in the Claude scratchpad, not in ~/.config/obs-studio (Fred's own OBS setup was never touched).
