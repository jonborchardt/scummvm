# Prompt 2 (actual fixing of commit history):

First read CLAUDE.md, especially the "Stage 3: Fork structure & upstreaming" section — it is authoritative and overrides any framing in this prompt that conflicts with it. Roger is a display-layer provider inside the existing SCI engine, not a new engine.

I have a working but messy branch of ScummVM changes. The code works, but the commit history is not reviewable. I do not want to cherry-pick old commits directly because they contain mixed concerns.

Help me manufacture a clean upstreamable branch from the final working diff.

## **Constraints**

Treat my current branch as raw material, not as a commit history to preserve.

Most intended feature code lives under engines/sci/roger/; the remainder is mechanical hook sites in engines/sci/ plus the generic .rin input driver (event.cpp, gui/EventRecorder.h).

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

Produce a file-by-file diff summary from upstream to my current branch (start from the inventory in CLAUDE.md Stage 3 and verify it).

Categorize each changed file:

* Roger provider code (engines/sci/roger/)  
* hook sites / provider wiring / build wiring in engines/sci/  
* generic core/helper change (the input driver)  
* Roger-specific leakage outside engines/sci/roger/ beyond mechanical hook sites  
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

  ## **Suggested commit slices**

* Add the abstract art-provider interface and provider registration/build wiring in SCI.  
* Add the mechanical hook sites at SCI's rendering/UI seams (null-guarded provider calls).  
* Add the Roger generation pipeline (pic parser, omyac, scaling, asset gen, cache).  
* Add the compositor, present path, and view cache.  
* Add UI/text capture and the hires text path.  
* Add the launcher/picker and config knobs.  
* Add docs and data layout notes.

The .rin scripted-input driver (event.cpp, gui/EventRecorder.h, roger\_input) goes on a separate PR branch as its own generic commit — it is independently pitchable upstream.

## **Important**

If a file contains mixed changes, split it with patch staging.

If a hunk contains mixed changes, ask me before manually editing the hunk.

Do not include game assets or proprietary data in the ScummVM source branch.

Flag any change outside engines/sci/roger/ as a potential upstream risk, except the documented mechanical hook sites — verify those stay mechanical (no Roger logic inline, no FileRogerArtProvider references in SCI code).

Do not produce one giant squashed provider commit unless I explicitly ask.

For a future submission, prefer one coherent provider/seam PR with several clean commits over many tiny incomplete PRs.

For generic non-engine changes (the input driver), prefer separate small PR branches with one clean commit each where practical.
