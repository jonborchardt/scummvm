# Fork Maintenance Guide

This document covers the day-to-day maintenance of this downstream ScummVM fork,
including how the repository is structured relative to upstream, the invariants
that keep the diff mergeable, how to track upstream changes, and guidance for
deploying fork builds.

## Fork model

This repository is a downstream fork of [`scummvm/scummvm`](https://github.com/scummvm/scummvm).
Its purpose is to ship the Roger display-layer enhancement for SCI0 EGA games
(SQ3, QFG1 EGA) while keeping a credible path to eventually upstreaming the
observer seam into the main project.

**Roger is a display-layer provider inside the existing SCI engine.** It has no
engine class, no `MetaEngine`, and no detection tables — game detection stays
entirely within SCI's existing infrastructure. The fork must never duplicate
`engines/sci/` into a second engine: doing so would break SCI's detection,
would be rejected upstream outright, and converts a maintained ~1000-line diff
into a whole-engine merge burden.

The full inventory of changes outside `engines/sci/roger/` (the hook sites, the
observer seam, the wiring in `sci.cpp`, etc.) is maintained in
[`FORK_AUDIT.md`](FORK_AUDIT.md). Keep that inventory current whenever hook
sites change.

## Remote and branch layout

```
origin    git@github.com:jonborchardt/scummvm.git   (fetch and push — the fork)
upstream  git@github.com:scummvm/scummvm.git        (fetch only — push deliberately disabled)
```

`master` is a **pristine mirror of `upstream/master`**. It never carries local
work, never receives feature merges, and is never used as the base for upstream
pull requests. Refresh it with:

```sh
git checkout master
git fetch upstream
git reset --hard upstream/master
git push origin master
```

The hard reset is safe only because of the never-carries-work invariant: if that
invariant is broken, the reset will silently discard local commits. Do not develop
on `master`, and do not merge feature branches into it.

Day-to-day work happens on long-lived development branches (the `jon-*` lineage).
The current deploy line is **merge-maintained and never rebased** — it must always
stay deployable. Development history on these branches is raw material, not a
reviewable record.

**Upstream PR branches are manufactured fresh** from `upstream/master` whenever a
slice of work is ready for upstream review (see [`UPSTREAMING_PLAN.md`](UPSTREAMING_PLAN.md)
and [`PR_PLAN.md`](PR_PLAN.md) for the planned slices). Three ways to bring work over:

- A clean existing commit: `git cherry-pick <hash>`
- A commit that needs reorganizing: `git cherry-pick --no-commit <hash>`, then
  `git reset`, `git add -p`, `git commit`
- Selected final files: `git checkout <dev-branch> -- path/to/file.cpp`, then
  `git add -p`, `git commit`

**History surgery** (rebasing or squashing the deploy line) requires an explicit
decision and a backup branch or tag before any destructive operation.

## Invariants that keep the fork mergeable

These rules must hold on every change; they are what make the diff reviewable and
the eventual upstreaming feasible.

**Hook sites stay mechanical.** Changes in `engines/sci/**` outside `engines/sci/roger/`
are limited to null-guarded `g_sciGfxObserver` event calls on the neutral
`SciGfxObserver` interface (`engines/sci/sci_gfx_observer.h`). SCI code never
names the concrete provider and never includes a `roger/` path. Verify with:

```sh
git grep -in "roger" -- engines/sci ':(exclude)engines/sci/roger'
```

The result must show only upstream game text and `module.mk` object-path lines.

**No changes to other engines.** SCI with the observer null must remain
byte-identical to stock ScummVM behavior.

**No game assets or proprietary data in the repository.** Game data and the
generation cache live in sibling directories outside the repo. See
[`DATA_LAYOUT.md`](DATA_LAYOUT.md) for the layout and the cache-version bump
discipline that must be followed whenever the generation pipeline changes.

**New source files carry standard ScummVM GPL headers** and follow upstream style.
The conventions are documented locally in `docs/scumm/` (code formatting,
coding conventions, commit guidelines).

## Tracking upstream

Refresh `master` from upstream periodically (recipe above). To integrate upstream
changes into the deploy line, **merge `master` into the development branch** (merge,
not rebase — the deploy line is never rebased):

```sh
git checkout <dev-branch>
git merge master
```

Conflicts should concentrate in the roughly twenty hook-site files inventoried in
[`FORK_AUDIT.md`](FORK_AUDIT.md). The `engines/sci/roger/` subtree is add-only
relative to upstream and should merge clean.

After any upstream merge, verify the build and run the three verification layers:

1. **Unit tests:** `.\build_tests.ps1`
2. **Smoke run:** `.\build_and_run.ps1 -Script test/sci/roger/scripts/qfg1-smoke.rin`
3. **Regression suite:** `.\test\sci\roger\run-regression.ps1`

Re-run the quarantine grep (above) to confirm no Roger references leaked into SCI
hook sites.

## Release and deployment orientation

**Binary name.** The build currently produces the stock executable name
`scummvm.exe`. A fork release intended for end users should ship under a
distinguishing name or version string so it cannot be mistaken for an official
ScummVM release. No specific brand is prescribed here; the important point is that
end users and support channels can tell the fork apart from upstream.

**GPL corresponding-source obligation.** Distributing binaries triggers the
obligation to make the exact corresponding source available. Fork releases must
point at the tagged commit or branch they were built from. See [`LEGAL.md`](LEGAL.md)
for the full licensing notes.

**Game data.** Users must provide their own legally obtained game copies. The
in-app Roger game picker handles per-game setup and precaching once game paths
are configured. See [`DATA_LAYOUT.md`](DATA_LAYOUT.md) for directory layout details.

**Supported platform.** Windows (MSVC, Visual Studio 2019 or 2022 with the
"Desktop development with C++" workload) is the developed and regularly tested
platform. The `devtools/create_project` tool generates the Visual Studio solution.
The standard ScummVM `configure`/`make` path on Linux and macOS is not regularly
exercised by this fork and should be treated as unverified.

**What remains manual.** The following steps have no automated packaging today:

- There is no installer or packaged release artifact; distribution is a bare
  `scummvm.exe` and its runtime libraries.
- Game paths are configured via the ScummVM launcher or `scummvm.ini`; there is
  no guided first-run setup beyond the Roger picker.
- Upstream merges are performed manually by the maintainer.
- Regression evidence review (capture diffs, walk-perf telemetry) is manual.
