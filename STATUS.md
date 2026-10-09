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
- Tech design drafted 2026-10-09 (docs/tech-design.md, ADR 0003, proposed): MOBS is an OBS plugin in mobs/ with a record dock and an edit dock, reusing Monologue's editor source by path, with the hotkey over D-Bus. OBS does all recording; Monologue's camera capture is dropped. Neither obs/ nor monologue/ is edited.
- Biggest unknown: a QQuickWidget (QML) inside an OBS dock on Wayland. Fallback: the editor opens in its own window from the same plugin.
- Next: practice copy (Test Drive VM with Omarchy, OBS 32, Qt), then the dock spike, then the screen design, then the plugin. Fred approved the tech design 2026-10-09; hotkey is physical Alt + Shift + V (pause: Ctrl + Alt + Shift + V), checked free.
