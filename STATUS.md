# mobs: status

- Started: 2026-10-09, from Clarity's Idea Box
- Stage: shape
- Done 2026-10-09: forked omacom/monologue and obsproject/obs-studio to nixfred; MOBS repo now holds both as git subtrees (obs/, monologue/) with full history and the official upstreams as remotes. See docs/adr/0002-one-repo-two-upstreams.md. Branch: import-upstreams (not merged to master, not pushed).
- Finding: Monologue is a webcam recorder with a trim editor (Qt 6 QML, C++), not a voice tool. Voxtype (already on Omarchy) is the dictation tool.
- Next: Make a practice copy first (Clarity backlog: “Make a practice copy first”), then shape how the two apps join (open question: Monologue as an OBS dock/plugin vs side by side), and the one-key start/stop.
