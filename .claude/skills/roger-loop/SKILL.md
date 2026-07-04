---
name: roger-loop
description: Use when a Roger/SCI change needs in-game verification or evidence — confirming a fix visually, capturing what a room or dialog looks like, checking walking-speed/perf telemetry, or reproducing a rendering bug in QFG1/SQ3 without asking the user to play.
---

# Roger verify-fix loop

Drive the game yourself, read the pixels, iterate. The mechanics (.rin grammar, harness switches, automation rules) live in CLAUDE.md under "Autonomous verification loop" — this skill is the procedure around them.

## The loop

1. **Scenario** — decide the minimal action sequence that proves or disproves the claim, and which capture labels are the evidence. One claim per script; always end with `quit`.
2. **Script** — write the `.rin` to the scratchpad (keep it out of the repo unless it's a reusable regression script → `test/sci/roger/scripts/`). Skeleton:

   ```
   wait 4000            # boot + save-restore settle
   capture before
   move 160 100         # EVERY capture needs a flushing move+wait after it — see below
   wait 600
   # actions...          walk needs ~2500ms, dialog appear ~2500ms
   capture after
   move 150 100
   wait 600
   quit
   ```

   **`capture` only pends; a present consumes it.** An idle scene (ego standing still, no
   dialog) presents nothing, so a bare `capture` before `quit` silently produces NO file.
   This is not dialog-specific — treat `capture <label>` → `move X Y` → `wait 600` as one
   atomic unit, always.

3. **Run** — `.\build_and_run.ps1 -Game qfg1 -SaveSlot 1 -Script <path> -TimeoutSec 120 [-NoBuild] [-CycleLog]`
   - **Always pass `-TimeoutSec`.** Exit 124 = hung script, killed for you — never a run you have to hunt down and kill.
   - `-NoBuild` whenever no C++ changed since the last build — script iteration drops to seconds.
   - The harness auto-deletes the previous log and this script's labelled captures, so whatever exists afterwards is this run's evidence.
4. **Evidence** — Read every capture PNG and state what you SEE in each. Exit code 0 proves the run finished, never the claim. If anything is off, grep `screenshots\roger-run.log` for `ROGER-SCRIPT` (parse errors, `log` output) and `ROGER-CYCLE`.
5. **Verdict or iterate** — code fix → rebuild (drop `-NoBuild` once) → rerun; script fix → keep `-NoBuild` and rerun.

## Aiming clicks at unknown targets

Coordinates are game-space 320×200 and the `-preview.png` is exactly the game rect (no letterbox). Run a capture-only script first, Read the PNG, locate the target as a fraction of image width/height, then `x = fx * 320`, `y = fy * 200`. Don't guess coordinates for anything smaller than a building.

## Moving between rooms

- **Clicks cannot cross a screen edge** — the ego walks to the clicked point and stops AT the
  edge without exiting. Burned three runs discovering this. To leave through an edge, use an
  arrow key: `key DOWN` (UP/LEFT/RIGHT) starts continuous SCI0 keyboard walking that carries
  the ego off-screen. After a room change the ego may stop; press the arrow again to resume.
- **Doors**: click ON the door (walk-to + touch enters). Clicking "toward" it (top edge above
  the building) can route the ego somewhere else entirely.
- **The capture filename tells you which room you're in**: captures land as
  `roger-<pic>-<label>-*.png`, so the pic id is your transition probe — a label you expected
  in the next room still prefixed with the old pic means the exit didn't fire. Budget
  generously: one screen crossing ≈ 8–12 s of `wait`.

## Recipes

| Goal | How |
|---|---|
| Capture an open dialog | `key ENTER` → `wait 2500` → `capture dlg` → `move 200 120` → `wait 400` → `key ENTER` |
| Ghost-text check | after dismissing: `wait 1200` → `move 160 100` → `capture gone` |
| **Roger bug or game behavior?** | launch with `-Mode sbs` — boots straight into Side-by-Side, every capture is an enhanced-vs-native comparison, no F10 choreography or mode restore. Left pane = enhanced, right = native mirror. Anything missing/extra on the left only is a Roger bug; identical on both = original game behavior. One such capture settled both "missing signs" (Roger-side) and "invisible ego" (native-legit occlusion). Mid-run mode switches are still `key F10` (cycles Enhanced → Original → Side-by-Side); `-Mode original` gives a native-only run. |
| Structured trace instead of pixels | add `-Diag` to the launch (sets the `ROGER_DIAG` env var for that process only). Never flip `roger_diag` in `scummvm.ini` for this — ini edits race a running instance's config rewrite-on-exit and need manual cleanup. Grep `ROGER-DIAG\[` in `screenshots\roger-run.log`. For capture/lifetime bugs the trace names exact view/loop/cel/rect/owner — often more decisive than screenshots. |
| Walking-speed / perf | add `-CycleLog`; grep `ROGER-CYCLE`; period ≈83 ms healthy, ≥150 suspicious, ≈225 = the historic bitsRestore regression |
| Watch motion mid-action | several `capture step<N>` spaced by `wait 800` (each with its flush move) |
| Interactive probing | `-Live cmd.txt` (background run, PID printed); append `.rin` lines to the file; finish with a `quit` line |
| Full regression gate (present-barrier phases) | `powershell -File test\sci\roger\run-regression.ps1` — one all-green run = gate pass. On a FAIL, inspect the saved capture PNGs for the failing entry BEFORE rerunning: the gate drives a live, focus-stealing game window, so stray keystrokes/clicks on the machine contaminate captures (seen 2026-07-04: a typed "fontsss" opened an SQ3 input dialog in a `same:` baseline capture — 477k px "regression"). Environmental cause visible in the diff → rerun and disregard; no environmental cause → treat as a real regression. `-Record` re-baselines perf (deliberate act only). |

## Known state

- Save slot 1 exists in both `qfg1` (room 300, town, ego near the sheriff's office) and `sq3`. The user may create more mid-session — verify a slot exists before blaming the script: `%APPDATA%\ScummVM\Saved games\<target>.0NN`. Need a different starting point? Ask the user for a save rather than scripting a long navigation.
- Targets: `qfg1`, `sq3` (Roger is EGA SCI0 only).
- The parser input line renders only while the game awaits input — a `typed` capture fired while the ego is still walking shows nothing; let movement finish first.
- A Side-by-Side `-preview.png` can exceed the Read tool's image size limit ("media removed"). Downscale to half size first (PowerShell `System.Drawing`: load, `DrawImage` into a half-size bitmap, save to the scratchpad) and Read that.

## Verdict discipline

- PASS requires naming the captures and what you saw in each — a claim per image.
- "Couldn't verify" is a valid verdict and is not FAIL: say which evidence is missing and why.
- Captures missing entirely? Check in order: exit code 124 (hang)? `ROGER-SCRIPT` parse warnings in the log? A `capture` with no flushing `move` after it (the #1 cause)? Did the script reach that label (bisect with `log <marker>` lines)? Still dark → the `-Diag` recipe above.
