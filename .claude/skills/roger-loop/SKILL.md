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
   # actions...          walk needs ~2500ms, dialog appear ~2500ms
   capture after
   quit
   ```

3. **Run** — `.\build_and_run.ps1 -Game qfg1 -SaveSlot 1 -Script <path> -TimeoutSec 120 [-NoBuild] [-CycleLog]`
   - **Always pass `-TimeoutSec`.** Exit 124 = hung script, killed for you — never a run you have to hunt down and kill.
   - `-NoBuild` whenever no C++ changed since the last build — script iteration drops to seconds.
   - The harness auto-deletes the previous log and this script's labelled captures, so whatever exists afterwards is this run's evidence.
4. **Evidence** — Read every capture PNG and state what you SEE in each. Exit code 0 proves the run finished, never the claim. If anything is off, grep `screenshots\roger-run.log` for `ROGER-SCRIPT` (parse errors, `log` output) and `ROGER-CYCLE`.
5. **Verdict or iterate** — code fix → rebuild (drop `-NoBuild` once) → rerun; script fix → keep `-NoBuild` and rerun.

## Aiming clicks at unknown targets

Coordinates are game-space 320×200 and the `-preview.png` is exactly the game rect (no letterbox). Run a capture-only script first, Read the PNG, locate the target as a fraction of image width/height, then `x = fx * 320`, `y = fy * 200`. Don't guess coordinates for anything smaller than a building.

## Recipes

| Goal | How |
|---|---|
| Capture an open dialog | `key ENTER` → `wait 2500` → `capture dlg` → `move 200 120` → `wait 400` (the move flushes the pending capture — without it you get the post-dismiss frame) → `key ENTER` |
| Ghost-text check | after dismissing: `wait 1200` → `move 160 100` → `capture gone` |
| Walking-speed / perf | add `-CycleLog`; grep `ROGER-CYCLE`; period ≈83 ms healthy, ≥150 suspicious, ≈225 = the historic bitsRestore regression |
| Watch motion mid-action | several `capture step<N>` spaced by `wait 800` |
| Interactive probing | `-Live cmd.txt` (background run, PID printed); append `.rin` lines to the file; finish with a `quit` line |

## Known state

- Save slot 1 exists in both `qfg1` (room 300, town, ego near the sheriff's office) and `sq3`. Need a different starting point? Ask the user for a save rather than scripting a long navigation.
- Targets: `qfg1`, `sq3` (Roger is EGA SCI0 only).
- The parser input line renders only while the game awaits input — a `typed` capture fired while the ego is still walking shows nothing; let movement finish first.

## Verdict discipline

- PASS requires naming the captures and what you saw in each — a claim per image.
- "Couldn't verify" is a valid verdict and is not FAIL: say which evidence is missing and why.
- Captures missing entirely? Check in order: exit code 124 (hang)? `ROGER-SCRIPT` parse warnings in the log? Did the script reach that label (bisect with `log <marker>` lines)? Still dark → set `roger_diag=true` for the target and grep `ROGER-DIAG[`.
