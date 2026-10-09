# 2. One repo, two upstreams, as git subtrees

- Date: 2026-10-09
- Status: decided (Fred, 2026-10-09)

## Decision

MOBS is a new project in this repo. It carries ALL of OBS Studio's features and ALL of
Monologue's features, and it must keep pulling the official commits from both, for good.

- `obs/` is `obsproject/obs-studio`, imported as a git subtree with full history.
- `monologue/` is `omacom/monologue`, imported as a git subtree with full history.
- Remotes `upstream-obs` and `upstream-monologue` point at the OFFICIAL repos. Their push URLs
  are set to `DISABLED_never_push_upstream` so nothing can go there by accident.
- Reference clones of our forks (`nixfred/obs-studio`, `nixfred/monologue`) sit in `forks/`,
  git-ignored. They are only for sending patches back upstream as PRs. MOBS does not depend on them.
- MOBS's own code goes OUTSIDE `obs/` and `monologue/` (for example `mobs/`), so upstream merges
  stay conflict-free. Changes inside the two subtrees are kept small and rebased on each pull.
- Licence: OBS is GPLv2, Monologue is MIT. The combined work is GPLv2.

## Pull updates (no script needed)

```sh
git subtree pull --prefix=obs        upstream-obs        master -m "Pull obsproject/obs-studio"
git subtree pull --prefix=monologue  upstream-monologue  master -m "Pull omacom/monologue"
git submodule update --init   # only when OBS bumped obs-browser / obs-websocket / libdshowcapture
```

A pull-forward was tested on a throwaway worktree on 2026-10-09: Monologue imported 3 commits
behind, pulled to head, tree identical. A real OBS update has not been exercised yet.

## Why

Choices considered: a real fork of OBS (huge C codebase, chase updates forever), a small wrapper
(smaller, but not "all features in one project"), and subtrees. Subtrees give one buildable repo
with both histories visible in `git log`, and each upstream merges in with a single command.
Cost: `.git` is about 88 MB, and OBS's three submodules need the root `.gitmodules` re-mapping.

## Consequences

- Never `git subtree add` again for an existing prefix; always `pull`.
- Do not squash subtree pulls: the point is to see upstream's real commits.
- OBS is Qt 6 widgets; Monologue is Qt 6 QML and C++17. How the two join into one app is the next
  open design question (see STATUS.md).
