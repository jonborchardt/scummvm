# Fork Audit: changes outside engines/sci/roger/

**Branch:** jon-update-core-audit (off jon-update-core) vs origin/master. **Date:** 2026-07-10.
**Companion design spec:** docs/superpowers/specs/2026-07-09-sci-gfx-observer-generalization-design.md (untracked working doc).
**Purpose:** classify every out-of-roger change for defensibility, inventory every SCI graphics seam, and prove the generalized observer redesign shrinks the diff (baseline 830 lines; cap 1000).

Re-measured 2026-07-10 at 14509d438c3; plan's 2026-07-09 figures were 825/18/83.

## 1. Methodology

Buckets: **N** notification / **C** claim-override / **S** state-intrusion /
**G** generic-fix / **D** dev-tool / **W** wiring.
Seam classifications: **hooked** / **derivable-from-L2** / **real-gap** / **excluded**.
Gap-fill rule: a real-gap needs BOTH content-not-in-L2-pixels AND a non-Roger consumer story.

## 2. Measured baseline

| File | Lines changed | Hunks |
|---|---|---|
| engines/sci/engine/kgraphics.cpp | 26 | 6 |
| engines/sci/event.cpp | 52 | 4 |
| engines/sci/graphics/animate.cpp | 74 | 14 |
| engines/sci/graphics/controls16.cpp | 94 | 9 |
| engines/sci/graphics/cursor.cpp | 3 | 2 |
| engines/sci/graphics/menu.cpp | 141 | 9 |
| engines/sci/graphics/menu.h | 22 | 3 |
| engines/sci/graphics/paint16.cpp | 167 | 17 |
| engines/sci/graphics/paint16.h | 4 | 1 |
| engines/sci/graphics/ports.cpp | 54 | 2 |
| engines/sci/graphics/scifont.cpp | 2 | 2 |
| engines/sci/graphics/scifont.h | 5 | 1 |
| engines/sci/graphics/text16.cpp | 44 | 4 |
| engines/sci/graphics/transitions.cpp | 27 | 2 |
| engines/sci/module.mk | 36 | 1 |
| engines/sci/sci.cpp | 67 | 4 |
| gui/EventRecorder.h | 6 | 1 |
| test/module.mk | 6 | 1 |
| **Total** | **830** | **83** |

## 3. Change-site classification

<!-- Tasks 2-5 append per-file subsections here -->

## 4. Seam inventory

<!-- Tasks 6-7 append per-class subsections here -->

## 5. L2 completeness verification

<!-- Task 9 -->

## 6. Consolidation table and line budget

<!-- Task 8 -->

## 7. Gap list

<!-- Task 8 -->

## 8. Standalone upstream PR candidates (G bucket)

<!-- Task 5 seeds, Task 10 finalizes -->

## 9. Findings that contradict the design spec

<!-- any task may append here; Task 10 resolves -->
