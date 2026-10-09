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
- Open question for the tech design: how Monologue (Qt 6 QML) joins OBS (Qt 6 widgets): dock, plugin, or something else, keeping MOBS's own code outside obs/ and monologue/.
- Next: make a practice copy first (Clarity backlog: “Make a practice copy first”) before any screen work, then the screen design, then the tech design.
