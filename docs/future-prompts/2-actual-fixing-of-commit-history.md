# Prompt 2 (actual fixing of commit history):

First read CLAUDE.md, especially the "Stage 3: Fork structure & upstreaming" section — it is authoritative and overrides any framing in this prompt that conflicts with it. Roger is a display-layer provider inside the existing SCI engine, not a new engine.

Also read the local ScummVM guideline copies in docs/scumm/ (code-formatting-conventions.md, coding-conventions.md, commit-guidelines.md). Prompts 0 and 1 should already have run: the code conforms to those guidelines, stale/wrong comments and any Claude/AI references have been scrubbed from code and upstream-facing docs, and the structure audit is done (already done as of 2026-07-11: `docs/roger/FORK_AUDIT.md` is the complete inventory, with re-measure notes; UPSTREAMING_PLAN.md / PR_PLAN.md are still placeholders per the docs/roger/README.md table) — so the slices below should need NO style fixes. If you find drift while slicing (style, a stale comment, or a stray Claude/AI reference), put the fix in its own separate style-only commit — never mixed into a functional slice.

I have a working but messy branch of ScummVM changes. The code works, but the commit history is not reviewable. I do not want to cherry-pick old commits directly because they contain mixed concerns.

Help me manufacture a clean upstreamable branch from the final working diff.

## **Constraints**

Treat my current branch as raw material, not as a commit history to preserve.

Most intended feature code lives under engines/sci/roger/; the remainder is mechanical hook sites in engines/sci/ plus the generic .rin input driver (engines/sci/event.cpp, gui/EventRecorder.h) and one fork-only exception: base/main.cpp's `PLUGIN_ENABLED_STATIC(SCI)`-guarded picker hook (~13 lines, the standalone-launcher seam — documented in FORK_AUDIT §3.14; it never enters a clean slice).

Do not delete or rewrite anything until you have created a backup branch/tag.

Do not commit anything without first showing me the staged diff and explaining the slice.

Keep the deploy branch (the jon-\* lineage) working; create a separate clean branch for upstreamable commits.

Use merge-based maintenance for the long-lived deploy branch — never rebase it.

Use rebase/interactive rebase only for short-lived upstream PR branches.

Primary goal: keep Roger deployable.

Secondary goal: create a plausible clean commit series for future ScummVM upstreaming.

## **Workflow I want**

Identify the upstream base branch (origin/master).

Create a backup branch/tag for the current state.

Produce a file-by-file diff summary from upstream to my current branch (already done as of 2026-07-11: start from `docs/roger/FORK_AUDIT.md` — complete inventory against merge-base 049ad2ed464, last re-measured at 21 files, 1060(+)/15(−) = 1075 raw outside engines/sci/roger/ — and just verify it is still current; diff against the merge-base, not plain origin/master, since upstream drift makes the latter noisy).

Categorize each changed file:

* Roger provider code (engines/sci/roger/)  
* hook sites / provider wiring / build wiring in engines/sci/  
* generic core/helper change (the input driver)  
* Roger-specific leakage outside engines/sci/roger/ beyond mechanical hook sites (expected as of 2026-07-11: none in engines/sci — the grep gate `git grep -in "roger" -- engines/sci ':(exclude)engines/sci/roger'` shows only upstream game text and module.mk object paths; the one documented exception is base/main.cpp's guarded picker hook, fork-only, excluded from slices)  
* docs/legal/release packaging  
* debug/prototype/hack  
* downstream-only dev tooling (build scripts, CLAUDE.md, .claude/, .gitignore, docs/superpowers/)  
* asset/data files that should not be in the ScummVM source repo (expected: none — test fixtures are tiny synthetic files and stay)

Create a clean branch from upstream.

Copy over only the desired final Roger files and required hook-site/wiring changes.

Propose a commit slicing plan before staging anything.

Stage changes one logical slice at a time using patch-level staging where needed.

For each proposed commit:

* show the staged diff summary  
* explain why this commit is coherent  
* run the relevant build/test command if available (build\_tests.ps1 for unit tests; build\_and\_run.ps1 \-Script \<file.rin\> for smoke — this is a Windows/MSVC environment, not configure/make)  
* wait for my approval before committing

Commit messages on the clean branch must follow docs/scumm/commit-guidelines.md: first line `SUBSYSTEM: Short summary` (≤50 chars, present tense — `SCI:` or `SCI: ROGER:` for the seam/provider work, `GUI`/`ALL` as appropriate for the input driver), blank line, body wrapped at ~72 chars, message meaningful without the diff. Every commit must compile (bisectability), no merge commits. **Commits on the clean/upstream branches must not mention Claude or AI agents in any form** — no attribution footers, no `Co-Authored-By` trailers, no references in subject or body. The deploy-branch attribution footer is a deploy-line convention only and never crosses over; verify each message before committing.

  ## **Suggested commit slices**

* Add the neutral observer interface and registration/build wiring in SCI (in the fork this landed 2026-07-10 as the `SciGfxObserver` seam — `engines/sci/sci_gfx_observer.{h,cpp}`, `setSciGfxObserver()`/`createSciGfxObserver()`; the slice re-manufactures it on the clean branch).  
* Add the mechanical hook sites at SCI's rendering/UI seams (null-guarded `g_sciGfxObserver` calls).  
* Add the Roger generation pipeline (pic parser, omyac, scaling, asset gen, cache).  
* Add the compositor, present path, and view cache.  
* Add UI/text capture and the hires text path.  
* Add the launcher/picker and config knobs.  
* Add docs and data layout notes (README/how-to, examples, data layout, license notes — updated as needed and free of Claude/AI references; downstream-only docs like CLAUDE.md, .claude/, docs/superpowers/ never enter a slice).

The .rin scripted-input driver (event.cpp, gui/EventRecorder.h, roger\_input) goes on a separate PR branch as its own generic commit — it is independently pitchable upstream.

## **Important**

If a file contains mixed changes, split it with patch staging.

If a hunk contains mixed changes, ask me before manually editing the hunk.

Do not include game assets or proprietary data in the ScummVM source branch.

Flag any change outside engines/sci/roger/ as a potential upstream risk, except the documented mechanical hook sites — verify those stay mechanical (no Roger logic inline, no concrete-provider/Roger references in SCI code: `git grep -in "roger" -- engines/sci ':(exclude)engines/sci/roger'` must show only upstream game text and module.mk object paths — verified clean 2026-07-11).

Before committing each slice, grep the staged content for Claude/AI-agent references and obviously stale comments (both should already be gone after Prompt 0) — nothing of the kind may reach the clean branch, in file content or commit message.

Do not produce one giant squashed provider commit unless I explicitly ask.

For a future submission, prefer one coherent provider/seam PR with several clean commits over many tiny incomplete PRs.

For generic non-engine changes (the input driver), prefer separate small PR branches with one clean commit each where practical.
