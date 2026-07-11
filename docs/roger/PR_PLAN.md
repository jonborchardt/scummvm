# Roger PR Plan

This document breaks the upstream-facing Roger work into concrete pull-request
slices. It is the concrete companion to [`UPSTREAMING_PLAN.md`](UPSTREAMING_PLAN.md)
(strategy, acceptance risk, sequencing). The measured inventory each slice draws
from is [`FORK_AUDIT.md`](FORK_AUDIT.md) — the slice contents below cite its
section and row identifiers rather than re-deriving them. The mechanics of
manufacturing an upstream branch from `upstream/master` are in
[`FORK_MAINTENANCE.md`](FORK_MAINTENANCE.md); the commit rules are
[`../scumm/commit-guidelines.md`](../scumm/commit-guidelines.md).

Upstream branches are **manufactured fresh from `upstream/master`** and contain
only the commits for one reviewable PR. They are not cherry-picked wholesale
from the deploy line: the deploy line's history is raw material (see the History
hygiene note at the end).

## Ground rules

Every slice below, and any future pass that touches this history, must satisfy
these rules. They are inherited from
[`../scumm/commit-guidelines.md`](../scumm/commit-guidelines.md) and from the
fork's own naming policy.

- **Every commit compiles on its own** (bisectability). A regression can then be
  located by bisecting the history.
- **No unrelated changes per commit.** One logical change per commit; commit
  small related groups of files with a tailored message, not many files under a
  vague one.
- **Never mix style/whitespace with functional changes** in one commit. This is
  called out explicitly in the guidelines and is easy to violate when reshaping
  hook sites.
- **No merge commits.** Upstream enforces linear history; manufactured branches
  are always fresh from `upstream/master`.
- **Commit-message format.** First line `SUBSYSTEM: Short summary`, **50 chars
  or less**, **present tense** ("Add hook", not "Added hook"); then a blank
  line; then a body wrapped at about 72 characters. `SUBSYSTEM` is `SCI:` for
  all seam and provider work (the two-level `SCI: ROGER:` form is acceptable for
  provider-internal commits, matching upstream practice); `GUI:` / `DOXYGEN:`
  for the input driver's common-code pieces; `DOCS:` for documentation.
  Messages must make sense without the diff.
- **No AI-agent or assistant references in upstream commits — ever.** Upstream
  PR commits must not mention Claude, AI agents, or any assistant workflow in
  any form: no attribution footers, no co-author trailers, no references in the
  subject or body. The fork deploy branch's attribution footer is stripped
  during manufacture and nothing similar is reintroduced. (The fork-internal
  documents may reference their own maintenance tooling; the *manufactured
  upstream commits* must not.)
- **User-facing changes need accompanying DOCS commits** (the guidelines
  require documentation for user-facing and common-code changes).
- **Common-code contributions need Doxygen comments.** Code outside `engines/`
  and `backends/` — which is exactly what the input-driver slice touches
  (`gui/`, `event.cpp`) — requires JavaDoc-style Doxygen comments on new
  methods, signature changes, and new classes. These may land under the
  `DOXYGEN:` subsystem.

## The slices

Five candidate PRs, in submission order. Sizes are approximate and drawn from
the FORK_AUDIT measurements.

### PR-A — Scripted-input driver (generic testing facility)

- **What.** The `.rin` scripted-input driver: a registered backend
  `EventSource` that injects events through the normal `pollEvent` path, plus
  the small `event.cpp` and `gui/EventRecorder.h` seam that hosts it. Pitched
  as a **generic headless scripted-input / testing facility** complementing
  EventRecorder — deliberately engine-agnostic, with no SCI includes.
- **Contents.** The engine-agnostic driver source (moved into a neutral
  location for the PR, not `roger/`), the `event.cpp` `EventSource`
  registration, and the `gui/EventRecorder.h` touch. Grounded in FORK_AUDIT's
  E-bucket input rows and the W2 EventRecorder row (§3, §8). Excludes anything
  Roger-specific.
- **Size.** Small — the driver plus a few lines of seam.
- **Commits (2).**
  1. `GUI: Add scripted headless input EventSource`
  2. `DOXYGEN: Document scripted input EventSource`
     (common-code Doxygen requirement; may fold into commit 1 if upstream
     prefers, but the guidelines allow a separate `DOXYGEN:` commit).
- **Dependencies / ordering.** None. Independent of the seam and of Roger; can
  go first as cheap goodwill.
- **Open questions.** Where upstream wants a generic input driver to live
  (`common/`, `gui/`, `backends/`); whether it should share code with
  EventRecorder rather than sit beside it. Resolve in the talk-first phase.

### PR-B — Standalone SCI/GUI fixes (G bucket)

Four independent fixes, each Roger-independent, bundled or split as upstream
prefers. All are from FORK_AUDIT §8.

- **B1 — scifont `ENABLE_SCI32` un-gate** (FORK_AUDIT F1 + F2). Remove the
  `#ifdef ENABLE_SCI32` around `GfxFontFromResource::drawToBuffer` (definition
  in `scifont.cpp` plus the override declaration in `scifont.h`). Both halves
  are one PR (both sides of un-gating the same method). Trivial, ~7 lines.
- **B2 — EventRecorder.h decl fix** (FORK_AUDIT W2). Declare
  `isImGuiRecorderEnabled()` unconditionally to match its unconditional
  definition and unguarded call sites. Trivial, ~6 lines. Note this file also
  appears in PR-A; if both PRs are live at once, whichever lands first carries
  the EventRecorder.h touch and the other rebases.
- **B3 — window caption from detection** (FORK_AUDIT S3). `SciEngine::run` sets
  the OS window caption via `EngineMan.findTarget` (full canonical title, no
  hardcoded strings), carrying the `engines/metaengine.h` include. Small,
  ~12 lines. **Must be pitched on its Roger-independent merit** — the
  stale/series-level-caption case — not as a Roger prerequisite.
- **B4 — text16 `textHeight = 0` init** (FORK_AUDIT T2). One-line initializer
  silencing a real uninitialized-read path.
- **Size.** Trivial to small overall.
- **Commits.** One commit per fix, each `SCI: <present-tense summary>`
  (e.g. `SCI: Un-gate GfxFontFromResource::drawToBuffer`). Four commits if
  submitted as one PR; equally valid as four tiny PRs.
- **Dependencies / ordering.** None between them (except the B2/PR-A file
  overlap noted above). Independent of the seam and of Roger; goes early with
  PR-A as goodwill.
- **Open questions.** Whether upstream wants these as one grouped PR or four
  separate ones; the B3 justification wording (must stand without Roger).

### PR-C — Neutral observer seam (the core PR)

- **What.** The mechanical, null-guarded SCI hook sites plus the neutral
  engine-owned observer interface that lets a display-layer provider attach
  without changing game logic. This is the reshaped seam described by
  FORK_AUDIT §6 (rows R1–R23) — the ~1000-line rule-adjusted diff reshaped
  toward the ≤ 830-line consolidation target.
- **Contents.** `engines/sci/sci_gfx_observer.{h,cpp}` (the interface, token
  scheme, and marshalling helpers — FORK_AUDIT R23) plus the mechanical hook
  calls in the roughly twenty SCI files inventoried in §2/§3: `paint16`,
  `animate`, `controls16`, `menu`, `text16`, `ports`, `transitions`,
  `kgraphics`, `cursor`, `palette16`, and the registration in `sci.cpp` /
  `module.mk`. The seam must land in its **reshaped** form (consolidated events
  per §6, fork-only telemetry and dev-toggle rows deleted per R19/R20, menu
  state exiled per R5), not the raw deploy-line form. The palette gap-fill hook
  (FORK_AUDIT §7, one hook at the `copySysPaletteToScreen` funnel) is part of
  this slice.
- **Excludes.** No Roger provider code, no provider types named in SCI code, no
  fork-only telemetry (`ROGER-DIAG`/`ROGER-CYCLE`), no dev-toggle/hotkey block,
  no `base/main.cpp` launcher seam, no tests (tests accompany PR-D).
- **Size.** ~1000 lines rule-adjusted across ~20 files, targeting ≤ 830 after
  reshape (FORK_AUDIT §6, §10 criterion 4).
- **Commits (a few clean ones, roughly 4).**
  1. `SCI: Add neutral graphics observer interface`
     (the `sci_gfx_observer.{h,cpp}` header/impl, registration wiring —
     compiles with no callers yet).
  2. `SCI: Emit observer events from graphics primitives`
     (the paint16/animate/controls16/text16/ports/transitions/cursor/palette16
     hook calls — mechanical, null-guarded).
  3. `SCI: Route menu state through the graphics observer`
     (the menu.{cpp,h} exile — the largest single reshape, R5; kept separate
     because it carries the most behavior and the most review risk).
  4. `SCI: Wire graphics observer registration`
     (`sci.cpp` / `module.mk` registration; the neutral factory).

  The exact split is negotiable, but each commit must compile on its own and
  none may mix style with function. Before any upstream slice, two known
  blockers from the audit must be cleared: the `FORBIDDEN_SYMBOL_EXCEPTION_getenv`
  in `sci.cpp` (S1) replaced with ConfMan/CLI, and the non-const function-static
  `s_prevCycleT0` (A14) removed with its telemetry.
- **Dependencies / ordering.** Gated on the talk-first seam decision
  (UPSTREAMING_PLAN sequencing step 3). Should follow PR-A/PR-B so the fork has
  already demonstrated it follows upstream conventions. B4 (text16 init) is
  logically upstream of C but not a hard dependency.
- **Open questions.** Whether the seam ships unconditionally (its current,
  byte-identical-when-null form) or behind a configure gate — a possible
  upstream ask, not the status quo. The final commit split. Whether the
  reshaped line count lands under 830 after the reshape work is actually done
  (the audit projects it does; the raw diff today does not — §10 criterion 4).

### PR-D — Roger provider (only if the seam is accepted)

- **What.** The provider itself: the entire `engines/sci/roger/` subtree
  (generation pipeline, compositor, overlay, view cache, launcher) plus its
  unit tests. Meaningful only after PR-C lands upstream.
- **Contents.** `engines/sci/roger/**` and the accompanying `test/sci/roger/**`
  suites. Upstream `test/` hosts engine tests, so the fork's CxxTest suites
  **accompany** this submission rather than being dropped — they are excluded
  only from the seam-only slices (PR-C), not from the provider PR. Dev
  utilities under `engines/sci/roger/utils/` are a judgment call (see the
  downstream-only list below); the recommendation is to keep them downstream
  and submit the provider without them, so the PR is scoped to the shipping
  feature rather than debug tooling.
- **Size.** Large (the bulk of the fork's added lines).
- **Commits.** Broken by subsystem within the provider — generation pipeline,
  compositor/overlay, launcher, tests — each `SCI: ROGER: <summary>` and each
  compiling on its own. The exact breakdown is designed when (if) this slice
  becomes live; it is not planned in detail here because it is gated on PR-C.
- **Dependencies / ordering.** Hard dependency on PR-C being accepted upstream.
  Last and largest.
- **Open questions.** Whether upstream wants the whole provider at once or in
  stages (background replacement, then compositor, then launcher); testing
  expectations for the soak-verified invalidation paths; whether any dev
  utilities should join.

### PR-E — Documentation

- **What.** The user-facing and developer documentation that must accompany the
  user-facing changes per the commit guidelines.
- **Contents.** `docs/roger.md` (the user-facing feature description) and any
  `docs/roger/` pieces that describe upstream-relevant behavior. Fork-internal
  maintenance documents (FORK_AUDIT, FORK_MAINTENANCE, this plan) stay
  downstream — see the list below.
- **Size.** Small to moderate.
- **Commits (1).** `DOCS: Document SCI display-layer enhancement`.
- **Dependencies / ordering.** Accompanies PR-C (for the seam/config surface it
  documents) and/or PR-D (for the provider feature). Per the guidelines,
  user-facing changes and their docs may be separate commits but should land
  together.
- **Open questions.** How much of `docs/roger.md` is upstream-relevant vs
  fork-specific (the fork harness knobs and `build_and_run.ps1` references are
  downstream-only and must be trimmed from the upstream doc).

## Downstream-only (never in any upstream set)

These are carried on the fork's deploy line and never appear in a manufactured
upstream branch:

- **`build_and_run.ps1`, `build_tests.ps1`** — fork dev harness.
- **`CLAUDE.md`, `.claude/`** — fork developer/orientation tooling.
- **`.gitignore` fork additions** — ignore rules for the sibling cache and the
  `screenshots/` directory.
- **`docs/superpowers/`** — gitignored / untracked; cannot leak into a diff.
- **`base/main.cpp` launcher seam** — the fork-only launcher replacement
  (`PLUGIN_ENABLED_STATIC(SCI)`-guarded picker hook, FORK_AUDIT §3.14 B1/B2).
  Replaces the stock launcher by default; not upstreamable as-is.
- **Root `README` fork note** — the fork orientation blurb (and the
  `engines/sci/README.md` fork note); never upstream as-is.
- **Fork-internal `docs/roger/` pieces** — `FORK_AUDIT.md`,
  `FORK_MAINTENANCE.md`, this `PR_PLAN.md`, `DATA_LAYOUT.md`, `LEGAL.md`. These
  are the fork's maintenance record, not upstream content. (Judged
  individually: `docs/roger.md` is the exception — it is user-facing and goes
  up with PR-E.)
- **Dev utilities under `engines/sci/roger/utils/`** — Roger Studio, the F12
  tune panel, and the Eye Exam tool. **Recommendation: keep them downstream.**
  They are debug/tuning tooling, not part of the shipping display feature; they
  inflate the provider PR with code no end user runs and no reviewer needs to
  assess to accept the feature. If upstream specifically wants a tuning surface
  for the pipeline they can be offered later as a separate, clearly-scoped
  contribution — but they should not ride along in PR-D by default.

## History hygiene note

The upstream branches are **manufactured fresh from `upstream/master`**, not
cherry-picked from the deploy line. This is a deliberate choice, and this note
records why, so no future pass mistakes it for an oversight.

The deploy line's commit history is **raw material, not a reviewable record**.
It contains classes of content that must never leak into a manufactured upstream
commit:

- **Debug iterations and prototype commits** — intermediate states that do not
  compile cleanly on their own or that were superseded within a few commits.
  These violate the bisectability rule if cherry-picked directly.
- **Reverted experiments** — approaches tried and backed out. Cherry-picking the
  history would replay dead ends; manufacturing from the final diff carries only
  the outcome.
- **Mixed style-and-function commits** — the deploy line did not always separate
  whitespace/formatting from functional change; upstream forbids the mix.
- **Fork dev-tooling commits** — changes to `build_*.ps1`, `.claude/`,
  fork-internal docs, and the `base/main.cpp` launcher seam are interleaved
  through the deploy history and must be excluded entirely.
- **Attribution footers** — the deploy branch's commit footer must be stripped;
  nothing equivalent is reintroduced.

If someone tried to build an upstream branch by cherry-picking the existing
deploy-line commits instead of manufacturing from the final diff, they would
have to split every mixed commit, squash or drop every superseded/reverted
iteration, strip footers, and filter out the dev-tooling commits — commit by
commit, across a long merge-maintained history. Manufacturing a fresh branch
from the final diff (checkout the final files, then `git add -p` into clean,
purpose-built commits per the slices above) is both cheaper and produces the
linear, single-purpose, compiles-on-its-own history upstream requires. That is
why manufacture, not per-commit surgery, is the chosen approach — and why this
plan describes slices and commit *targets* rather than a remap of existing
commit hashes.
