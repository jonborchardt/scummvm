# Prompt 0 (Conform fork code to the ScummVM guidelines):

First read CLAUDE.md, especially the "Stage 3: Fork structure & upstreaming" section — it is authoritative and overrides any framing in this prompt that conflicts with it.

Then read the local copies of the ScummVM contribution guidelines in docs/scumm/:

* docs/scumm/code-formatting-conventions.md
* docs/scumm/coding-conventions.md
* docs/scumm/commit-guidelines.md

These are snapshots of the wiki pages (the wiki itself is bot-gated and fetch tools fail; the snapshots were verified against the wiki text on 2026-07-02). They are the rulebook for this pass.

## Why this pass exists and why it runs first

The commit guidelines forbid mixing style/whitespace changes with functional changes in one commit. Prompt 2 will manufacture a clean upstreamable branch from the final diff — if style drift is still present at that point, every manufactured slice either carries style noise or needs splitting. Fixing conformance now, as its own pass on the working deploy branch, means:

* Prompt 1's audit (docs/roger/FORK_AUDIT.md) evaluates already-conformant code and stays focused on structure, not style.
* Prompt 2's manufactured commits are purely functional.
* Upstream review never sees convention drift (fixing it in the fork beforehand is cheaper than during review — CLAUDE.md Stage 3 says exactly this).

Note this pass runs on the **current branch, in place** — it is file updates only. All branch/history restructuring is deferred to Prompts 1 and 2.

## Hard rules (non-negotiable)

1. **Never alter a file this branch has not already modified.** If a file is identical to origin/master, it is off-limits — no matter how badly it violates the guidelines. Upstream's own drift is upstream's problem.
2. **Within a modified file, only alter the chunks this branch added or changed.** Use the per-file diff vs origin/master to identify our hunks; untouched upstream code in the same file stays byte-identical, even a line away from a violation. (This is also why `.clang-format` must never be run against whole files — it would reformat upstream code we didn't touch.)
3. **No git surgery of any kind.** Stay on the current branch. No new branches, no rebasing, no amending, no history rewriting, no fork restructuring. This pass only updates files; changes land as ordinary commits on the current branch, nothing more.

## Scope — the fork diff only

Only touch code this fork added or modified (diff vs origin/master). **Never do janitorial sweeps of untouched upstream code** — that bloats the diff Prompts 1 and 2 have to reason about, which is the opposite of the goal.

Targets, in priority order:

1. `engines/sci/roger/**` — the provider (moves wholesale to any upstream PR)
2. Hook sites in `engines/sci/**` (paint16, animate, controls16, menu, ports, text16, transitions, kgraphics, scifont — see the Stage 3 diff inventory)
3. The generic `.rin` input driver (`common/events`-adjacent code in `event.cpp`, `gui/EventRecorder.h`) — held to the *stricter* common-code bar (Doxygen required)
4. `test/sci/roger/**` (CxxTest sources)

Exempt: downstream-only dev tooling (build_and_run.ps1, build_tests.ps1, CLAUDE.md, .claude/, .gitignore, docs/superpowers/) — the C++ rules don't apply and these never go upstream.

## Conformance checklist

Work through each target file against the docs/scumm/ rules. The high-value checks:

**Formatting (code-formatting-conventions.md):**

* Tabs for indentation, width 4; attached (hugging) braces
* Pointer/reference aligned right: `int *ptr`, `void foo(int &bar)`; no space after casts
* Spaces around binary operators, after keywords and commas
* No composite one-liners (`if (x) doThing();`); mandatory `{}` on empty loop bodies
* Switch fall-through marked with exactly `// fall through`
* Preprocessor directives start at column 0
* Naming: `CamelCase` types, `camelCase` functions/methods/locals, `_camelCase` members, `g_camelCase` globals, constants `kCamelCase` or `ALL_CAPS` (prefer enum/`const` over `#define`)
* `.clang-format` is a backstop, but review its diffs manually — do not blindly reformat whole upstream files it happens to touch

**Portability / conventions (coding-conventions.md):**

* No forbidden symbols (`printf`, `fopen`, `getenv`, `rand`, `sprintf`, ...) — compiler-enforced by `common/forbidden.h`; verify no `FORBIDDEN_SYMBOL_EXCEPTION_*` was ever added
* No C++ exceptions, no global objects with constructors (POD/pointer globals like `g_sciRogerProvider` are fine)
* **No non-const static locals inside function bodies** — strictly forbidden (return-to-launcher reentrancy). The two known Roger violations (roger_studio.cpp warn-once flag, file_roger_art_provider.cpp diag-dedup signature) were already fixed by moving to member state; grep `static (bool|int|uint32)` under `engines/sci/roger/` and the hook files to verify none crept back
* Non-const globals need a justification comment saying why + where they are re-set at engine start (else `// FIXME: non-const global var`)
* Endian-safe data access (`READ_LE_UINT32` etc. or stream `readUint32LE` methods — never pointer-cast struct overlays); packed structs only via `common/pack-start.h`/`pack-end.h` + `PACKED_STRUCT`
* Use `Common::` classes directly (the `Std::` wrappers are only for ported STL codebases); file access via `Common::File`/SaveFileManager only

**Comments / documentation:**

* `FIXME` / `TODO` / `WORKAROUND` used per their defined meanings; every WORKAROUND must explain what original-game bug it works around
* Doxygen (JavaDoc style, `@param`, `@` not `\`) on the common-code additions outside engines/ — this is a hard requirement for the input-driver code in `gui/`/event.cpp; encouraged but not required inside `engines/sci/roger/`
* Sweep for stray debug leftovers, commented-out prototype code, and dead TODOs in the diff
* **Remove stale or wrong comments** in the fork diff: comments that describe removed behavior or an abandoned approach, narrate change history ("was X, now Y", "fixed in this commit"), restate the adjacent code, or make claims the code no longer backs. A comment earns its place only by stating a constraint the code can't show. (Same scope rules apply: only our hunks — never touch upstream comments.)
* **No references to Claude, AI agents, or assistant tooling** in code comments, identifiers, strings, commit-bound docs, or anything else in the fork diff. Describe what the code does in neutral engineering terms. Such references are permitted only in downstream-only files that need them (CLAUDE.md, .claude/, docs/superpowers/ specs/plans) — all already exempt from this pass and never upstreamed.
* If a removed/corrected comment shows that user-facing docs (docs/roger.md, READMEs, examples) repeat the same stale claim, fix the doc in the same style-only slice or flag it in the report

## Process

1. Build the diff file list vs origin/master (start from the CLAUDE.md Stage 3 inventory) and bucket files into the targets above.
2. Sweep each bucket with targeted greps + reading; fix violations.
3. **Commit style-only fixes separately from any functional fix you happen to discover** — never mix, per the commit guidelines. Use `SCI: ROGER:` / `SCI:` prefixed, present-tense subject lines ≤50 chars. The deploy-branch attribution footer stays for now on this branch; eventual upstream PR commits must not mention Claude or AI agents anywhere — no attribution footers, no co-author trailers, no references in subject or body (Prompt 2 enforces this when manufacturing the clean branch).
4. If you find something that looks like a bug (not style), flag it and ask before fixing — bug fixes are functional changes and belong in their own commit with their own verification.
5. Verify after the sweep: `build_and_run.ps1` builds and the smoke script runs (`-Script test/sci/roger/scripts/qfg1-smoke.rin` or the SQ3 smoke), and `build_tests.ps1` unit tests pass. This is a Windows/MSVC environment — do not assume configure/make.
6. Update the CLAUDE.md Stage 3 diff inventory if line counts shifted materially, and note in CLAUDE.md that the conformance pass has run.

## Output expected

* Conformant fork diff (targets above), committed in style-only slices on the current branch
* A short report: what was fixed per bucket (including stale comments removed and any Claude/AI references scrubbed), anything intentionally left non-conformant and why, and any flagged functional issues deferred to their own commits
* Green build + unit tests + smoke run

Do not rewrite commit history in this pass — these are ordinary working-tree fixes committed normally on the deploy line. History work is Prompt 2's job.
