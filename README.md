# mobs

Fork a voice tool and OBS (a big screen-recording program) into one Omarchy-only app called MOBS. It's worth a tiny version, not a full merge.

## Where it came from

From Clarity's Idea Box: control-plane #10 “Go fork https://github.com/omacom/monologue and…”.

### The idea, in its own words

> Go fork https://github.com/omacom/monologue and https://github.com/obsproject/obs-studio and lets combine them in to a project called MOBS that take the best of both of them and make it Omarchy specific.

### The first look's review

- **What he's really after**: One Omarchy-native way to record your screen and talk to it, with narration or dictated notes, without juggling two separate tools.
- **Worth building?**: OBS is a huge program that a non-coder can't realistically maintain as a fork, but a small wrapper that starts recording and voice together could be useful.
- **Smallest version**: A one-key shortcut on Omarchy that starts and stops a screen recording with your voice captured and a text transcript saved next to it. About an afternoon.
- **Cost to run**: Nothing to run if the voice-to-text happens on your own computer; some plan usage while Claude builds it.
- **Risks**: Maintaining your own copy of OBS means chasing its updates forever; use OBS as-is and only add a small script around it.; I couldn't confirm what that Monologue repo does; the well-known Monologue is a Mac app, so check the repo before building on it.; Omarchy already has voice dictation (Voxtype) built in, so MOBS may only need to add the recording part.
- **Already his**: blip: already works across your Mac and Linux machine, so some of the Omarchy-side know-how is there
- **Overlap with his projects**: Both are Linux-side tools you run on Omarchy, so small helper scripts could be shared.

### Follow-up answers

- Approach: should MOBS be a real fork of both programs or a small wrapper around them?: A new project, set up so it can always pull commit updates from both official upstreams (answered 2026-10-09; see docs/adr/0002-one-repo-two-upstreams.md)
- Main use: what do you want to do most with MOBS?: ALL the features of OBS and Monologue, made into one big usable project (answered 2026-10-09)
- Control: how should you start and stop it?: A global hotkey (works from any app), plus record and pause buttons in the MOBS window (answered 2026-10-09)

## What was agreed (shaping, 2026-10-09)

**Who it's for.** Just Fred, on his own Omarchy machines (gus and vic). It can sit on GitHub, but it is not designed around other people: no installer, no support promises.

**What it must do.** The destination is ALL of OBS Studio's features and ALL of Monologue's features (webcam recorder, pause marks, the trim and cut editor) in one app. The first working version is **one combined app from day one**: Monologue's recording and editing living inside the OBS window, not two programs glued together. How exactly (an OBS dock or plugin, or something else) is the tech design step's job.

**How you control it.** One global Hyprland hotkey starts and stops a recording from any app, even when MOBS isn't focused: physical **Alt + Shift + V** (pause: Ctrl + Alt + Shift + V). The window also has record and pause buttons, with Monologue's Space-to-record feel.

**What it looks like (Fred, 2026-10-09).** "Super easy for beginners but also satisfy experts. It looks too much like OBS right now. Put some Omarchy feel on it." So MOBS is **its own application in the live Omarchy theme**: one program with the OBS engine (libobs) built in, not a skin over the OBS app and not a window that drives a hidden OBS (ADR 0006: "I don't want an overlay. I want a new product."). A beginner sees three big cards (my screen, screen + me, just my camera) and then one Record button. Experts press `P` for a Pro drawer in the same look; everything OBS can do is reached through MOBS's own interface over the same engine. Pictures of the five screens: `docs/screen-design.md` (sample-data prototype, not a working recorder).

**What's left out of the first version.**

- Windows and macOS. MOBS is Omarchy-only. OBS's code for other systems stays in the tree untouched so upstream pulls still merge cleanly, but nothing is built or tested for it.
- Voice dictation and transcripts. Voxtype already does dictation on Omarchy; MOBS does not rebuild it.

Not ruled out, not planned first: live streaming (an OBS feature, so it stays in), and packaging or an installer (builds from the repo on gus and vic for now).

**Known risks, accepted.**

- "One combined app" means one program: MOBS's own frontend with libobs inside it (ADR 0006). Spike 4 proved the core: a Qt program with libobs embedded records with pause on Wayland. Still to prove: screen capture in-process, and a live preview. The honest cost is that MOBS owns more (sources, encoders, audio, saved settings).
- Keeping OBS and Monologue pulling from their official upstreams (ADR 0002) means MOBS's own code must live outside `obs/` and `monologue/`, and edits inside them stay small.
- Monologue's editor is reused by path (unmodified) in the review screen; the OBS engine (libobs and its plugins) is built from the `obs/` subtree without OBS's UI. Both upstreams stay untouched.

## Set-up choices

- Size: Medium (Adds a screen design and a tech design step.)
- Plans and reviews: Opus (Fable for a very large plan)
- Writes the code: Sonnet helpers, in working copies
- ChatGPT reviews only, read-only
- Chats: ask before risky actions (.claude/settings.json)
- Never send email, texts or invites without a yes
- A practice copy for testing screens: yes, as the first step
- Needs API keys: yes

## Status

Shaped (2026-10-09): who it's for, what it must do and what's left out are settled above. The tech design (`docs/tech-design.md`) and the screen design (`docs/screen-design.md`) are drafted and approved in direction. Since then (2026-10-10) the app is built and records for real: choose and ready screens, screen, webcam and mic capture with libobs in-process, a live preview, the review screen with Monologue's editor, and a Pro drawer. See STATUS.md.
