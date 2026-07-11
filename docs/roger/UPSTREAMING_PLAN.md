# Roger Upstreaming Plan

This document describes the strategy for contributing the Roger display-layer
work back to the main ScummVM project. It is the high-level companion to
[`PR_PLAN.md`](PR_PLAN.md), which breaks the work into concrete pull-request
slices. The measured inventory of the out-of-`roger/` diff that any upstream
slice must carry lives in [`FORK_AUDIT.md`](FORK_AUDIT.md); the branch and
remote model used to manufacture upstream branches is in
[`FORK_MAINTENANCE.md`](FORK_MAINTENANCE.md). The user-facing feature
description is [`../roger.md`](../roger.md).

## Overview

Roger is a **display-layer provider inside the existing SCI engine** — not a
new engine, not a new game. It renders high-resolution backgrounds and view
cels, generated in-engine from the game's own SCI resources, through ScummVM's
OSystem overlay while leaving all game logic at the native 320×200. It targets
EGA SCI0 games (SQ3, QFG1 EGA today) and is display-only.

This fork has two goals, in priority order:

1. **Primary — ship the fork.** The fork exists to deliver Roger to end users
   who own the original games. This goal does not depend on upstream accepting
   anything. If upstream declines every slice, the fallback is exactly this
   fork, maintained with a deliberately minimized diff against
   `upstream/master` — never a duplicated engine.

2. **Secondary — keep a credible upstream path.** Everything in the fork's
   out-of-`roger/` diff is written so that the mechanical seam *could* be
   offered to upstream cheaply if there is interest. The audit exists to keep
   that path open, not to force it.

### What "upstreamable" means here, concretely

The upstreamable part is **not** the whole Roger provider in the first
instance. It is the **neutral seam** that lets a display-layer provider attach
to SCI without changing game logic:

- **Minimal, mechanical hook sites.** Each SCI graphics chokepoint emits a
  single null-guarded event on a neutral, engine-owned observer interface
  (`SciGfxObserver`, `engines/sci/sci_gfx_observer.h`). No Roger types, no
  game-specific branches, no provider logic inline in SCI code.
- **Neutral naming.** SCI code names only the observer interface and the
  neutral registration factory. The concrete provider type appears nowhere
  outside `engines/sci/roger/` (grep-enforced; see FORK_AUDIT §11.4).
- **Byte-identical when off.** With no observer registered, every hook is a
  null check that falls through, so SCI renders exactly as stock. This is the
  central acceptance property: enabling the seam changes nothing until a
  provider opts in.

The provider itself (the `engines/sci/roger/` subtree and its tests) is a
larger, later, and more speculative submission that only makes sense once the
seam is accepted.

## Candidate map

Every file in the fork diff falls into one of two dispositions: an upstream
candidate (grouped into the PR slices of [`PR_PLAN.md`](PR_PLAN.md)) or
downstream-only. The table below is the compact map; the "why" column gives
the one-line rationale and the slice reference.

| Item | Disposition | Why |
|---|---|---|
| `.rin` scripted-input driver (`event.cpp` seam + `gui/EventRecorder.h`) | Upstream — PR-A | Generic headless scripted-input facility; engine-agnostic; independent of Roger |
| scifont `ENABLE_SCI32` un-gate (`scifont.{cpp,h}`, FORK_AUDIT F1+F2) | Upstream — PR-B | Standalone bug fix; no Roger dependency (FORK_AUDIT §8) |
| `EventRecorder.h` decl fix (FORK_AUDIT W2) | Upstream — PR-B | Standalone decl/definition mismatch fix; no Roger dependency (FORK_AUDIT §8) |
| Window-caption-from-detection (`sci.cpp`, FORK_AUDIT S3) | Upstream — PR-B | Standalone SCI fix with Roger-independent merit (FORK_AUDIT §8) |
| text16 `textHeight = 0` init (FORK_AUDIT T2) | Upstream — PR-B | One-line uninitialized-read fix (FORK_AUDIT §8) |
| Neutral observer seam (`sci_gfx_observer.{h,cpp}` + ~20 SCI hook-site files) | Upstream — PR-C (the core PR) | The reshaped ~1000-line mechanical seam; only meaningful upstream contribution that enables display-layer providers |
| Roger provider (`engines/sci/roger/`) + tests (`test/sci/roger/`) | Upstream — PR-D (only if seam accepted) | The provider itself; large; the fork's unit tests accompany it (upstream `test/` hosts engine tests) |
| User + developer docs (`docs/roger.md`, relevant `docs/roger/` pieces) | Upstream — PR-E | Required alongside user-facing changes per commit guidelines |
| Fork-only launcher seam (`base/main.cpp`) | Downstream-only | Replaces the stock launcher by default; not upstreamable as-is |
| Dev harness (`build_and_run.ps1`, `build_tests.ps1`, `.claude/`, `.gitignore` fork additions, root README fork note) | Downstream-only | Fork tooling; never part of an upstream PR |
| Fork-internal docs (`FORK_AUDIT`, `FORK_MAINTENANCE`, this plan, `PR_PLAN`, `DATA_LAYOUT`, `LEGAL`) | Downstream-only (judge each) | Fork-maintenance record, not upstream content |
| Dev utilities under `engines/sci/roger/utils/` (studio, tunepanel, eyetest) | Downstream-only by default | Debug tooling; see PR_PLAN for the recommendation |

The per-slice contents, sizes, commit breakdown, and ordering are in
[`PR_PLAN.md`](PR_PLAN.md).

## Scope and acceptance risk

ScummVM is primarily a **preservation-oriented project**: it replaces the
original game executables and runs the games from their original, legally
obtained data files. Roger's story fits that framing well — but it also pushes
on it in ways that upstream may want to discuss. This section is an honest
assessment of both.

### The favorable framing

Roger runs **original commercial games** (SQ3, QFG1 EGA) from their **original
data files**, as a **display-only, opt-in enhancement** that is
**byte-identical to stock when disabled**. It is not a new private game and not
a new engine. Framed this way, the acceptance question is "should SCI grow a
neutral seam for display-layer enhancement providers?", not "should ScummVM
adopt a private art mod?".

### What strengthens the pitch

- **Legally obtainable game data.** Users provide their own copies of the
  original commercial games; no game assets ship in the repo (verified: the
  only binaries in the diff are four synthetic test fixtures under 100 bytes
  each — FORK_AUDIT §11.5).
- **Cleanly separated engine code and generated cache.** All generation output
  is a content-hash cache in a sibling directory outside the repo, never
  committed. See [`DATA_LAYOUT.md`](DATA_LAYOUT.md).
- **No proprietary runtime dependency.** Everything is generated in-engine from
  SCI resources through ScummVM's own abstractions; there is no external
  binary, service, or asset pack.
- **A clear long-term maintainer.** The fork has an active maintainer committed
  to keeping the seam current against upstream (FORK_MAINTENANCE).
- **A portable implementation using ScummVM abstractions.** The overlay is
  ScummVM's OSystem overlay; text uses the font manager; images use the image
  decoders; the input driver is a backend `EventSource`. No platform-specific
  code paths in the seam.
- **A small, mechanical core diff.** The seam is null-guarded event calls; with
  the observer null the engine is byte-identical to stock. The reshaped
  projection targets ≤ 830 lines across the SCI hook sites (FORK_AUDIT §10).

### What weakens it / open risks

These are the concrete things a reviewer is likely to raise. They are grounded
in the audit where possible.

- **The seam adds around thirty virtuals touching hot paths of a mature
  engine.** The observer interface carries roughly thirty virtuals across four
  layers (frame lifecycle, pixel truth, semantic, claims — FORK_AUDIT §6 R23).
  Several sit on the per-cycle `kernelAnimate` path. Even null-guarded, that is
  new surface area in code that has been stable for a long time.
- **The measured diff exceeds the audit's own worst-case bound.** The reshaped
  projection is ≤ 830 lines, but the audit's post-implementation measurement of
  the *actual* (not yet reshaped) diff came in at roughly 1000 rule-adjusted
  lines across about twenty SCI files — over the audit's 650-line conservative
  robustness bound, though under the 1000 hard cap (FORK_AUDIT §10, criterion
  4). Most of the overage is load-bearing contract comments in the observer
  header, not structural bloat, but the honest number to give upstream is
  "about a thousand lines", and the reshape to the ≤ 830 target is work that
  still has to be done on a manufactured branch.
- **The enhancement is EGA-SCI0-only — two games today.** VGA and SCI1+ are out
  of scope by design and hard-rejected at startup. Upstream may reasonably
  question whether a seam justified by two games belongs in shared engine code,
  or whether the generality claim ("any display-layer provider") is real yet.
- **TTF-based UI text redraw could read as authenticity drift.** Roger redraws
  dialog and status text with bundled TrueType fonts rather than the original
  bitmap SCI font. For a preservation project this can look like drift from the
  original presentation, even though it is opt-in and display-only.
- **The seam is compiled in unconditionally today.** It is byte-identical to
  stock when the observer is null, but it is always present rather than gated
  behind a build flag. Upstream may prefer a configure-time gate (see the open
  questions below); that is a possible design change, not the current state.
- **Ongoing maintenance burden on SCI developers.** Hooks that only this
  provider uses still have to survive future SCI refactors. Some are subtle
  (owner-object promotion, per-line text rects, restore/reveal lifetime) and
  the invalidation behavior is verified by interactive soak rather than by an
  automated gate, which is harder for an upstream maintainer to reason about.
- **Precedent.** Accepting a display-layer seam in SCI invites other engines to
  ask for the same. That is a project-direction question upstream will want to
  answer deliberately, not have decided for them by one PR.

None of these is disqualifying under the favorable framing, but they are real,
and pretending otherwise would waste maintainer goodwill. The mitigation for
all of them is to raise them first, in conversation, before investing in
upstream polish.

### Questions to ask ScummVM maintainers early

ScummVM has a strong talk-first culture. These questions belong on
scummvm-devel or Discord **before** any seam PR is written, so the answers can
shape the submission rather than invalidate it:

1. **Is there interest in a display-enhancement provider seam in SCI at all?**
   This is the gating question; a no here means the fork stays downstream and
   the rest of the polish is skipped.
2. **Is a neutral, always-compiled, null-guarded observer seam acceptable, or
   is a configure-time gate (e.g. `--enable-sci-display-provider`) preferred?**
   The answer determines whether the seam ships unconditionally or behind a
   flag.
3. **Where should generated-cache data live, given the data-file policies?**
   Roger writes a content-hash cache in a sibling directory outside the repo;
   confirm that fits upstream's expectations for user-generated derived data.
4. **What are the testing expectations for a provider like this?** The fork's
   tests are CxxTest suites for the SCI-type-free units plus a scripted `.rin`
   smoke/regression harness; some invalidation behavior is soak-verified.
   Understanding what upstream needs to accept the seam (and later the
   provider) shapes what has to be built before submission.

## Sequencing

The sequence is chosen so the cheap, reversible steps come first and the
expensive, speculative steps come last — and so that nothing is built on polish
until upstream has signalled interest.

1. **Talk first (cheap, reversible).** Raise the questions above on
   scummvm-devel / Discord. No code is written or submitted at this stage; the
   fork is unaffected regardless of the answer.
2. **Small generic PRs to build credibility (cheap, reversible).** Submit the
   standalone fixes that stand on their own merit and do not mention Roger: the
   scripted-input driver (PR-A) and the G-bucket fixes (PR-B). These are useful
   to upstream independent of any seam decision, demonstrate that the fork
   follows upstream conventions, and can be withdrawn or left as-is with no
   downstream impact.
3. **Seam discussion, then the seam PR (moderate cost).** Only after the seam
   question is answered favorably, manufacture and submit the core observer-seam
   PR (PR-C). This is where the reshape-to-target work is spent; do not spend it
   before step 1 gives a green light.
4. **Provider (expensive, only if the seam lands).** The Roger provider and its
   tests (PR-D) and the accompanying docs (PR-E) are the last and largest step,
   meaningful only if the seam is accepted upstream.

Steps 1 and 2 are cheap and reversible and can proceed on their own timeline.
Steps 3 and 4 are gated on the outcome of step 1 and should not be started
speculatively. If upstream declines at any gate, the fork continues under its
primary goal with the diff kept minimal against `upstream/master`.
