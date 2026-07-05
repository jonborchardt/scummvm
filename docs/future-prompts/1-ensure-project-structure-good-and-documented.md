# Prompt 1 (Ensure project structure is good and documented): 

First read CLAUDE.md, especially the "Stage 3: Fork structure & upstreaming" section — it is authoritative and overrides any framing in this prompt that conflicts with it.

Prompt 0 (0-conform-code-to-scummvm-guidelines.md) should already have run: the fork diff is expected to conform to the ScummVM guidelines (local copies in docs/scumm/), stale/wrong comments removed, and no references to Claude or AI agents anywhere in code or upstream-facing docs. Assume that pass is done — spot-check rather than redo it, and if you notice drift (style, stale comments, or stray Claude/AI references), flag it in the audit report instead of fixing it inline (style fixes must never mix with this pass's structural changes).

I have an existing local ScummVM clone with many local commits on my current branch. The code currently works, but the project needs to be cleaned up so it supports two goals:

Primary goal: deploy my Roger display-layer provider in a downstream ScummVM fork.

Secondary goal: preserve a credible path to upstreaming Roger into ScummVM later.

## **Important framing**

This is not a drop-in plugin for stock ScummVM.

This should be treated as a downstream ScummVM fork with a display-layer enhancement inside the existing SCI engine. Roger is NOT a new engine: it has no engine class, no metaengine, no detection tables, and must never grow them — detection stays SCI's. Never fork engines/sci/ into a duplicated engine.

The Roger-specific implementation should live almost entirely under engines/sci/roger/.

Changes to the rest of engines/sci/ and ScummVM core should be minimized, isolated (mechanical null-guarded provider hook sites only), documented, and made generic where possible.

Assume the modified ScummVM code and the Roger code must be released under GPL-compatible terms.

Assume game assets/data may be separately licensed and should not be mixed into the GPL engine source.

Do not rewrite commit history or destructively rearrange branches in this pass unless I explicitly approve it.

Before deleting, moving, or heavily refactoring anything, show me the plan.

Please perform the following audit and restructuring work.

## **Step 1: Analyze the current branch**

Identify the upstream base branch (origin/master).

Inspect the full diff from upstream ScummVM. Start from the diff inventory already recorded in CLAUDE.md Stage 3 and verify/update it.

Categorize every changed file into one of these buckets:

* Roger provider code (engines/sci/roger/)  
* Required SCI hook sites / provider registration / build wiring  
* Generic reusable ScummVM core/helper changes (e.g. the .rin input driver)  
* Roger-specific changes currently leaking outside engines/sci/roger/ beyond mechanical hook sites  
* Temporary hacks/debug code/prototype code  
* Asset/data files that should not be in the engine source repo  
* Documentation/legal/release packaging changes/downstream-only dev tooling

Produce a short markdown report at docs/roger/FORK\_AUDIT.md listing each changed file, its category, and what should happen to it.

## **Step 2: Quarantine Roger-specific code**

Move or refactor Roger-specific logic into engines/sci/roger/ wherever possible.

Hook sites elsewhere in engines/sci/ are expected and allowed, but must stay mechanical: a null-guarded call to a virtual on the abstract provider (roger\_art\_provider.h) plus minimal argument marshalling — no Roger logic inline, and SCI code never names FileRogerArtProvider.

Do not modify any other engine. Do not modify ScummVM core behavior unless the change is generic and defensible independent of Roger.

If a non-engine change is required, make the smallest possible API/hook/helper, and document why it exists.

## **Step 3: Shape Roger like an upstreamable provider**

Normalize the layout so it resembles what upstream could accept: a neutral, engine-owned observer/provider seam in engines/sci/ (future setArtProvider() registration API per CLAUDE.md Stage 3), with Roger as the sole implementation, compiled out by default. There is no metaengine, detection table, or engine class to create — do not create them.

Prefer ScummVM common APIs for:

* filesystem access  
* graphics surfaces  
* audio mixer  
* save files  
* events  
* configuration  
* logging/debug channels  
* platform abstraction

Do not introduce platform-specific assumptions unless isolated behind ScummVM abstractions.

## **Step 4: Separate game data from engine source**

Identify any assets, generated data, proprietary data, or sample game data currently committed. (Expected state per CLAUDE.md: none — game data and the generation cache live in sibling directories outside the repo; test fixtures are tiny synthetic PNGs; screenshots are gitignored. Verify this is still true.)

Move anything that violates this out of the fork.

Add documentation explaining the expected game data directory layout (the \<game\>-roger sibling-directory convention and cache layout).

Make sure Roger loads data from the configured game directory rather than hardcoded paths.

## **Step 5: Documentation**

Add documentation under docs/roger/:

* README.md: what Roger is, how to build it, and how to run a game with it  
* FORK\_MAINTENANCE.md: how this downstream fork should be kept close to upstream ScummVM  
* UPSTREAMING\_PLAN.md: which changes are potential upstream candidates, which are downstream-only, and why  
* DATA\_LAYOUT.md: expected game data files, directory layout, cache versioning, and what is not included  
* LEGAL.md: GPL notes for modified ScummVM/Roger code, plus a clear statement that game data/assets are separately licensed

Also update existing user-facing material as needed so it stays consistent with the new docs/roger/ set: docs/roger.md (the how-to), any README-level notes, examples/sample commands, and license/COPYING references touched by the fork.

All documentation written or updated in this pass is upstream-facing unless explicitly downstream-only: write it in neutral engineering terms with no references to Claude, AI agents, or assistant workflows. Downstream-only files that legitimately need such references (CLAUDE.md, .claude/, docs/superpowers/ working specs/plans) stay out of any upstream-facing set.

## **Step 6: Legal hygiene**

Preserve ScummVM license headers and conventions.

Verify GPL headers on all Roger source files (they should already comply).

Do not mix proprietary assets/data into GPL source directories.

Add a top-level note or doc explaining that this fork distributes modified ScummVM code and therefore must provide corresponding source.

Keep game content licensing separate from engine code licensing.

## **Step 7: Commit/PR hygiene preparation**

Do not rewrite history destructively in this pass unless I explicitly approve it. Instead:

Produce a proposed commit slicing plan in docs/roger/PR\_PLAN.md. Slices must satisfy docs/scumm/commit-guidelines.md (every commit compiles, no mixed concerns, no style+functional mixing, `SUBSYSTEM:` message format) — note this in the plan so Prompt 2 inherits it. Also record in the plan that upstream PR commits must not mention Claude or AI agents in any form — no attribution footers, no co-author trailers, no references in subject or body (the deploy-branch footer is stripped and nothing similar is reintroduced).

Identify candidate upstream PRs:

* Pure generic core/build/helper changes (notably the .rin scripted-input driver in event.cpp \+ gui/EventRecorder.h, pitched as a headless testing facility complementing EventRecorder)  
* The neutral observer-seam hook sites in engines/sci/, if acceptable separately  
* The Roger provider itself  
* Documentation

Identify changes that should remain downstream-only (build\_and\_run.ps1, build\_tests.ps1, roger\_run.ps1, CLAUDE.md, .claude/, .gitignore, docs/superpowers/).

Identify large commits that should be split.

Identify prototype/hack commits that should be squashed or removed before public release.

Because the secondary goal is eventual ScummVM upstream inclusion, do not optimize only for my downstream fork. Also assess whether Roger is likely to be in-scope for ScummVM at all.

Add a section to docs/roger/UPSTREAMING\_PLAN.md called Scope and acceptance risk covering:

* ScummVM is primarily a preservation-oriented project that replaces original game executables and runs games from their original data files.  
* Roger's story is the favorable one: it runs original commercial games (SQ3, QFG1 EGA) from their original data, as a display-only, opt-in enhancement that is byte-identical to stock when disabled. Frame the risk assessment in these terms — NOT as a brand-new private game.  
* Identify what strengthens the pitch: legally obtainable game data; cleanly separated engine code and generated cache; no proprietary runtime dependency; clear long-term maintainer; portable implementation using ScummVM abstractions; minimal core changes (a small, mechanical hook-site diff).  
* Identify anything in the current implementation that makes upstream inclusion less likely.  
* Recommend questions to ask ScummVM maintainers early (scummvm-devel / Discord, talk-first culture), before investing too much in upstream polish.

  ## **Step 8: Build and test**

Ensure the fork builds locally. This is a Windows/MSVC environment: use build\_and\_run.ps1 for the build+launch path and build\_tests.ps1 for unit tests (the make-based path); do not assume configure/make works here.

Run the roger regression scripts (build\_and\_run.ps1 \-Script with test/sci/roger/scripts/) as the smoke test; add manual test instructions.

Add a "known limitations" section.

Confirm that existing ScummVM engines are not affected by my changes, and that SCI with the provider null (Roger off) is behavior-identical to stock.

## **Step 9: Release/deploy orientation**

Add notes for how to ship this as a custom ScummVM-based player:

* expected binary naming  
* source publication requirements  
* how users provide or install game data  
* what platforms are currently supported (Windows today)  
* what remains manual

  ## **Output expected**

* A cleaned project layout  
* Documentation files  
* A categorized audit of current changes  
* A proposed PR/commit slicing plan  
* A list of risky upstreaming issues  
* A list of downstream-only changes  
* A build/test summary
