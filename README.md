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

- Approach: should MOBS be a real fork of both programs or a small wrapper around them?: not answered
- Main use: what do you want to do most with MOBS?: not answered
- Control: how should you start and stop it?: not answered

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

Shaping: who it's for, what it must do, and what's left out. See STATUS.md.
