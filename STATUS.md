# mobs: status

- Started: 2026-10-09, from Clarity's Idea Box
- Stage: shape done 2026-10-09; next is the screen design and the tech design (Medium track)
- Done 2026-10-09: forked omacom/monologue and obsproject/obs-studio to nixfred; MOBS repo holds both as git subtrees (obs/, monologue/) with full history and the official upstreams as remotes. See docs/adr/0002-one-repo-two-upstreams.md. Merged to main and pushed to origin (github.com/nixfred/mobs).
- Finding: Monologue is a webcam recorder with a trim editor (Qt 6 QML, C++), not a voice tool. Voxtype (already on Omarchy) is the dictation tool.
- Shaped 2026-10-09 (details in README.md):
  - For: Fred only, on his Omarchy machines (gus, vic).
  - First version: one combined app from day one, with all of OBS's and Monologue's features as the destination.
  - Control: global hotkey plus record and pause buttons in the window.
  - Left out: Windows and macOS, voice dictation and transcripts.
  - Not ruled out: live streaming (stays in as an OBS feature), packaging (later).
- Risk: the combined app is the largest option. Fallback if it stalls: record with OBS, open the take in Monologue's editor.
- Tech design drafted 2026-10-09 (docs/tech-design.md, ADR 0003, decided): MOBS is an OBS plugin in mobs/ with a record dock and an edit dock, reusing Monologue's editor source by path, with the hotkey over D-Bus. OBS does all recording; Monologue's camera capture is dropped. Neither obs/ nor monologue/ is edited.
- Spike 1 PASSED 2026-10-09 (ADR 0004, mobs/spike/, docs/spike-1-dock-in-obs.png): Monologue's real EditBar.qml runs in an OBS dock on native Wayland. The default OpenGL Qt Quick path turned OBS's preview black; software rendering fixed it (A/B in a Test Drive VM). Not yet proven on real hardware (VM is virgl) or with video playback in the dock.
- Practice copy: Test Drive VM test-drive-mobs-spike (OBS 32.2.2, Qt 6.11.2, cmake/ninja added; its package DB was stale, refreshed with pacman -Sy).
- Next: spike 2 (play a take inside the dock without GL, check on gus hardware), then the screen design, then the plugin.
