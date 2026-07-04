# Roger Phase 2: Cycle-Diff Backstop Net — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the cycle-diff backstop net (spec §5 Phase 2): at the `snapshotNativeBaseline` seam, diff the native visual buffer against the previous cycle's copy and invalidate the changed boxes — so any native change that slips past the invalidation hooks heals within one cycle instead of ghosting — plus the overlay-truth capture mode Phase 1 proved is a prerequisite for validating any of it.

**Architecture:** All changes in `engines/sci/roger/` (plus test manifest/tooling); zero engine-side edits — the `snapshotNativeBaseline()` provider hook already runs at the right seam every cycle. Task 1 ships the overlay-truth capture mode (`.rin` captures read the bounded path's persistent scratch — a byte-for-byte overlay mirror — instead of forcing a full clean recompose) and uses it to prove the gate can now detect invalidation faults (closing Phase 1's formally-unmet exit criterion). Task 2 ships the net itself: prev-cycle visual snapshot → `extractChangedBoxes` → `markNativeDirty` per box (which already handles the status-strip clip, grow(1), coordinate mapping, and mid-cycle scene-deferred routing). Task 3 proves the heal property with the same fault injections. Task 4 is perf/dirty-area evidence and the twice-green exit.

**Tech Stack:** C++11 (no exceptions/RTTI, tabs, K&R), MSVC build via `.\build_and_run.ps1 -NoLaunch`, the regression gate `powershell -File test\sci\roger\run-regression.ps1` as the per-task acceptance test. No new CxxTest surface — the net composes two already-unit-tested pieces (`extractChangedBoxes`, `markNativeDirty`); the gate + telemetry are the executable verification.

## Global Constraints

- **Containment (spec §2, hard gate):** code changes in `engines/sci/roger/` only; `test/**`, `build_and_run.ps1` (downstream tooling), and docs are also allowed. **Zero** edits under `engines/sci/graphics/`, `common/`, top-level `graphics/`, `backends/`, `base/`, `gui/`. In particular: `GfxScreen` gets **no new accessor** — the net reads the visual buffer via the existing per-pixel `getVisual(x, y)` loop (the same loop Feeder B uses). If the measured cost exceeds budget, the documented fallback is spec §5's own: ship with `roger_diff_net=false` as the default and record the finding — NOT an engine edit. Verify per task: `git diff --stat <base>` shows only allowed paths.
- **Branch policy (spec §6):** one commit series on branch `jon-p2-diffnet`, created off the tip that contains Phase 1 (`jon-p1-barrier` @ `dbd4fb5fbcb` or later; if Phase 1 has been merged to `jon-refactor1` by execution time, branch off `jon-refactor1` instead). Rollback: the net has the runtime escape hatch `roger_diff_net=false`; the truth-capture mode is off by default and needs no hatch.
- **Perf thresholds (spec §7, enforced by the gate):** `ROGER-CYCLE` period median ≤ baseline × 1.05; busy median ≤ baseline + 1 ms; p90 ≤ baseline p90 × 1.10. Baselines (`test/sci/roger/baselines/perf-baseline.json`): qfg1 period=83 p90=84 busy=17; sq3 period=83 p90=84 busy=4. Known flake: busy-only red with medians on baseline → re-run once; **never re-record baselines**. Note the busy+1ms threshold is itself the hard backstop on the net's cost budget (sq3 busy=4 leaves no headroom for a >1 ms net).
- **Net cost budget (spec Phase 2):** snapshot+diff < 1.0 ms median. Measured via the `ROGER-NET sum32=<ms> boxes=<n>` line (sum of the last 32 cycles' cost in ms — `getMillis()` is too coarse per-cycle; the 32-cycle sum gives ~0.03 ms resolution). Budget: **sum32 ≤ 32**.
- **Dirty-area watch-item (Phase 1 lesson):** use **totalAreaPerCycle** (sum of `ROGER-PRESENT area` ÷ cycle count), NOT per-present medianArea — Phase 1's sq3 "+23.4%" scare was the median shifting composition when the barrier eliminated small presents. Task 4's totalAreaPerCycle must be ≤ Task 1's recorded numbers × 1.10 for both games.
- **Truth-capture is evidence-only:** `roger_truth_capture` / `ROGER_TRUTH_CAPTURE` defaults OFF. Normal gate runs and normal play are unaffected. The net (`roger_diff_net`) defaults **ON**.
- **Build:** `.\build_and_run.ps1 -NoLaunch` (~1–3 min incremental). **Gate:** `powershell -File test\sci\roger\run-regression.ps1` — 7 entries, 33 checks, ~10 min, exit 0 required. Give gate tool calls a 600000 ms timeout. Fault-injection scratch edits are NEVER committed; `git checkout -- <file>` + rebuild after each.
- Commits on `jon-p2-diffnet`; `docs/superpowers/**` needs `git add -f` (gitignored by design). **Do not commit** anything under `.superpowers/` or `screenshots/`.

## Code facts (verified 2026-07-03 at `dbd4fb5fbcb`, for every implementer)

Line numbers drift — match by function/member name.

- `snapshotNativeBaseline()` (`file_roger_art_provider.cpp:2123`): first statement is `_inAnimateCycle = true;` (line 2128), then the Feeder B gate `if (!_diffBackstop && _mode != Roger::kModeSideBySide) return;` and a per-pixel `screen->getVisual(x, y)` loop into `_nativeBaseline` (`Common::Array<byte>`), `_haveBaseline = true`. The net's block goes between the flag and that gate.
- `Roger::extractChangedBoxes(const byte *prev, const byte *cur, int w, int h, Common::Array<Common::Rect> &out)` (`roger_compositor.h:55`, impl `roger_compositor.cpp:224`): row-run scan + `coalesceDirtyRects`; empty `out` on identical buffers. Unit-tested in `test_compositor.h` (`test_extract_changed_boxes`, `test_extract_changed_boxes_identical_is_empty`).
- `markNativeDirty(const Common::Rect &nativeRect)` (`file_roger_art_provider.cpp:1272`): clips to `Common::Rect(0, _statusBarH, 320, 200)` (status strip transparent), `grow(1)`, `sciRectToDest`, `_compositor->addDirtyRect(dest)`, and — because `_inAnimateCycle` is true at the net's seam — `_compositor->addSceneDirtyRect(dest)` so the scene seed union re-seeds the region (the Phase 1 deferred-ghost fix). Exactly what a net box needs; call it per box, no new mapping code.
- `needFullSource` (`presentWithUi`, `file_roger_art_provider.cpp:1130`):
  `const bool needFullSource = (_inputDriver && _inputDriver->capturePending()) || _autoshot || _compositor->nextPresentIsFull();`
  The bounded path's tail runs `maybeScriptCapture(scene, _lastGameRect); // guaranteed no-op (needFullSource)`.
- **The persistent scratch is an overlay mirror.** `_scratchScene` is a reused buffer (realloc only on size change, `scratchScene(w,h)` at `file_roger_art_provider.cpp:1096`). `renderFrame` fully rewrites it each cycle; the bounded path copies into it exactly the regions it pushes; every present sources from it. So at any present, scratch == what the overlay shows. This is why truth capture = "stop forcing full source", nothing more. (Resize reallocs it, but resize also invalidates the composite cache → full path → full compose before any bounded present. `presentComparison` overwrites it with the sbs layout — which is the presented sbs frame, still truth.)
- `presentBarrier` fresh-frame branch (`file_roger_art_provider.cpp:1329`) already calls `maybeScriptCapture(scene, ...)` on the scratch — per-cycle captures are already overlay-truth. Only blocking-seam presents (through `presentWithUi`) force the full source today.
- `maybeScriptCapture` (`file_roger_art_provider.cpp:673`): `if (_inputDriver->takeCaptureRequest(label)) dumpAutoshot(scene, gameRect, ("-" + label).c_str());`. `InputScriptDriver::capturePending() const` (roger_input.h:118) is the non-consuming peek; the barrier's skip gate already refuses to skip while a capture pends.
- Env-first knob pattern (constructor, `file_roger_art_provider.cpp:106`):
  `const char *envDiag = getenv("ROGER_DIAG"); _diag = envDiag ? (Common::String(envDiag) != "0" && Common::String(envDiag) != "false") : (ConfMan.hasKey("roger_diag") && ConfMan.getBool("roger_diag"));`
  (`getenv` matches existing file idiom; the upstream-portability cleanup is a Stage-3 concern, not this plan's.) `_cycleLog` is read the same way from `ROGER_CYCLE_LOG` / `roger_cycle_log` (line 126).
- `build_and_run.ps1` env-switch pattern (lines 292–299): `if ($CycleLog) { $env:ROGER_CYCLE_LOG = "1"; Write-Host ... }` — `-TruthCap` copies this shape. The `param()` block ends with `[int]$TimeoutSec = 0`.
- Gate manifest: `test/sci/roger/regression-manifest.json`, entries `{name, target, saveSlot, script, timeoutSec, cycleLog, checks[]}`; check types `capturesExist`, `sameRunDiff`, `presenceDiff`, `perf`. `run-regression.ps1` invokes `build_and_run.ps1` per entry — env vars set in the calling PowerShell session (e.g. `$env:ROGER_TRUTH_CAPTURE = "1"`) are inherited by every entry's game run. **Unset them (`Remove-Item Env:ROGER_TRUTH_CAPTURE`) when done.**
- `ROGER-CYCLE period=<ms> busy=<ms>` is emitted in `animate.cpp:779` when `cycleLogEnabled()`; `ROGER-PRESENT full=<0|1> regions=<n> area=<px>` per present when `-CycleLog` (Phase 1 Task 1). Both land in `screenshots\roger-run.log`.
- `GfxScreen` exposes only per-pixel `getVisual(int16 x, int16 y)` (screen.h:430); `_visualScreen` is private; 320×200 = 64,000 reads/cycle (the Feeder B snapshot does the identical loop when enabled).
- Room-change seam: `pushHiresBackground` (`file_roger_art_provider.cpp:~455`) probes caps (`_statusBarH = _caps.statusBarRows;` line 462) and resets per-room capture state — the net's `_haveNetPrev = false` reset goes with that per-room reset block.
- Phase 1 injection targets (for Tasks 1 and 3): the `markNativeDirty(nativeRect);` first statement in `onNativeEraseRect`; the `markVacatedDirty(removedRects[i]);` loop body in `uiClearToken`; the `markNativeDirty(screenRect);` first statement in `onNativeShowRect`. Phase 1 result: removing ALL THREE left the gate green — because captures forced a full clean recompose (`needFullSource`), making invalidation faults structurally invisible. That is the fact Task 1 exists to fix.
- `Common::Rect::grow(int)` grows in place and returns void — never chain it.

---

### Task 1: Phase branch + overlay-truth capture mode + detection proof

The evidence channel first. After this task, a `.rin` capture (under the new opt-in mode) reads the frame the present path actually produced — so a missing invalidation mark finally shows up in a capture, and the gate's existing `sameRunDiff` checks can catch the class. The detection-proof step retroactively closes Phase 1's unmet exit criterion ("a deliberate fault injection is caught by the suite").

**Files:**
- Modify: `engines/sci/roger/file_roger_art_provider.h` (member `_truthCapture`)
- Modify: `engines/sci/roger/file_roger_art_provider.cpp` (knob read; `needFullSource`; comment)
- Modify: `build_and_run.ps1` (`-TruthCap` switch)
- Modify: `docs/roger.md` (knob table row)

**Interfaces:**
- Consumes: existing `capturePending()`, `needFullSource`, the persistent-scratch overlay-mirror property (Code facts).
- Produces: `_truthCapture` (bool member, default false; env `ROGER_TRUTH_CAPTURE` overrides ini `roger_truth_capture`); `build_and_run.ps1 -TruthCap`. Tasks 3–4 run evidence passes with `$env:ROGER_TRUTH_CAPTURE = "1"`. Also produces this plan's dirty-area baselines (median + totalAreaPerCycle) that Task 4 compares against.

- [ ] **Step 1: Create the phase branch**

```bash
# If Phase 1 is already merged into jon-refactor1, use jon-refactor1 as the start point instead.
git checkout jon-p1-barrier
git checkout -b jon-p2-diffnet
```

- [ ] **Step 2: Add the `_truthCapture` knob**

`file_roger_art_provider.h` — next to the other capture/diag knob members (`_autoshot`, `_diffBackstop`, `_diffCheck`; match their declaration style):

```cpp
	bool _truthCapture = false; // .rin captures read the bounded-path scratch (overlay mirror) instead of forcing a full recompose — evidence mode, default off
```

`file_roger_art_provider.cpp` constructor — directly after the `_diffCheck` read, following the `ROGER_DIAG` env-first idiom:

```cpp
	// Overlay-truth captures (spec Phase 2 / Phase 1 lesson): with this on, a pending
	// .rin capture no longer forces the full clean recompose — the capture is dumped
	// from the persistent scratch, which mirrors the overlay byte-for-byte. This is
	// the evidence channel for invalidation faults: Phase 1's injections A–D all
	// stayed green because needFullSource masked them from every capture.
	const char *envTruth = getenv("ROGER_TRUTH_CAPTURE");
	_truthCapture = envTruth ? (Common::String(envTruth) != "0" && Common::String(envTruth) != "false")
	                         : (ConfMan.hasKey("roger_truth_capture") && ConfMan.getBool("roger_truth_capture"));
```

- [ ] **Step 3: Stop forcing the full source when truth mode is on**

In `presentWithUi`, replace the `needFullSource` computation (Code facts show the current lines):

```cpp
	// The -ui autoshot dump reads the WHOLE present source, and a present that
	// presentToOverlay will decide to push FULL (heal frame / dirty-present off /
	// bg rebuild) needs a fully composed frame: the bounded path only makes the
	// pushed regions valid. A pending .rin capture also forces the full source —
	// UNLESS _truthCapture: then the capture reads the bounded path's scratch,
	// which mirrors the overlay byte-for-byte (every present writes scratch and
	// overlay identically), i.e. the frame the player actually sees.
	const bool captureForcesFull = _inputDriver && _inputDriver->capturePending() && !_truthCapture;
	const bool needFullSource = captureForcesFull || _autoshot ||
	                            _compositor->nextPresentIsFull();
```

and update the bounded path's tail comment (the code line is unchanged):

```cpp
		maybeScriptCapture(scene, _lastGameRect); // no-op unless _truthCapture (scratch mirrors the overlay)
```

- [ ] **Step 4: Add `-TruthCap` to build_and_run.ps1**

In the `param()` block, after `[int]$TimeoutSec  = 0` add (with a comma on the preceding line):

```powershell
    [switch]$TruthCap
```

Next to the `-CycleLog`/`-Diag` env blocks:

```powershell
if ($TruthCap) {
    $env:ROGER_TRUTH_CAPTURE = "1"
    Write-Host "Truth captures: .rin captures read the presented frame, not a forced full recompose" -ForegroundColor Cyan
}
```

- [ ] **Step 5: Document the knob**

`docs/roger.md`, in the config-knob table, after the `roger_diff_check` (or nearest diagnostic-knob) row:

```markdown
| `roger_truth_capture` | off | Evidence mode: `.rin` captures dump the frame the present path actually produced (the persistent scratch, a byte-for-byte overlay mirror) instead of forcing a full clean recompose. Required for fault-injection evidence — with it off, missing invalidation marks can never appear in a capture. Per-launch: `build_and_run.ps1 -TruthCap` (env `ROGER_TRUTH_CAPTURE`). |
```

- [ ] **Step 6: Build**

Run: `.\build_and_run.ps1 -NoLaunch`
Expected: build succeeds.

- [ ] **Step 7: Clean-build gate WITH truth captures — proves the bounded path's frames are correct**

```powershell
$env:ROGER_TRUTH_CAPTURE = "1"
powershell -File test\sci\roger\run-regression.ps1
Remove-Item Env:ROGER_TRUTH_CAPTURE
```

Expected: ALL PASS (33 checks). Any pixel-check FAIL here is a REAL Phase 1 latent bug (the presented frame differs from the clean recompose) — diagnose before proceeding (read the named `screenshots\roger-*-<label>-preview.png` captures; the diff region tells you which element/path), fix in this task, re-run.

- [ ] **Step 8: Record this plan's dirty-area baselines (median AND totalAreaPerCycle)**

```powershell
.\build_and_run.ps1 -NoBuild -Game qfg1 -SaveSlot 1 -Script test\sci\roger\scripts\qfg1-walk-perf.rin -TimeoutSec 90 -CycleLog
```

then compute and record:

```powershell
$areas = Select-String screenshots\roger-run.log -Pattern 'ROGER-PRESENT full=\d regions=\d+ area=(\d+)' |
    ForEach-Object { [double]$_.Matches[0].Groups[1].Value }
$cycles = (Select-String screenshots\roger-run.log -Pattern 'ROGER-CYCLE ').Count
$sorted = @($areas | Sort-Object)
$total = ($areas | Measure-Object -Sum).Sum
"presents=$($areas.Count) cycles=$cycles medianArea=$($sorted[[int][math]::Floor($sorted.Count/2)]) totalAreaPerCycle=$([math]::Round($total/[math]::Max(1,$cycles)))"
```

Repeat with `-Game sq3-1 ... -Script test\sci\roger\scripts\sq3-walk-perf.rin`. Paste both result lines into this plan's Addendum under "Task 1 dirty-area baseline (pre-net)". These are Task 4's ≤ ×1.10 reference (compare **totalAreaPerCycle**; record medianArea for context only).

- [ ] **Step 9: Detection proof — the gate must catch the invalidation-fault class under truth captures**

Scratch edit (NEVER committed) to `engines/sci/roger/file_roger_art_provider.cpp`: comment out all three Phase 1 injection targets — `markNativeDirty(nativeRect);` in `onNativeEraseRect`, the `markVacatedDirty(removedRects[i]);` loop body in `uiClearToken`, and `markNativeDirty(screenRect);` in `onNativeShowRect` (= Phase 1 injection D). Then:

```powershell
.\build_and_run.ps1 -NoLaunch
$env:ROGER_TRUTH_CAPTURE = "1"
powershell -File test\sci\roger\run-regression.ps1
Remove-Item Env:ROGER_TRUTH_CAPTURE
git checkout -- engines/sci/roger/file_roger_art_provider.cpp
.\build_and_run.ps1 -NoLaunch
```

Expected: **RED** — `sameRunDiff` FAILs in `qfg1-cmdbox` and/or `sq3-dismiss-matrix`/`sq3-wiggle` (stale pixels now visible in the truth frames). Record exactly which checks failed in the Addendum ("Detection proof"): this closes Phase 1's exit criterion.
**If it stays GREEN: STOP and escalate to the human.** That would mean even the presented frames show no staleness without the marks — the marks are redundant in every gate scenario and the gate needs a new scenario, which is a human scoping decision. Do not proceed to Task 2 on a green Step 9.

- [ ] **Step 10: Normal gate (truth mode off) — nothing changed for normal runs**

Run: `powershell -File test\sci\roger\run-regression.ps1`
Expected: ALL PASS (33 checks), exit 0.

- [ ] **Step 11: Commit**

```bash
git add engines/sci/roger/file_roger_art_provider.h engines/sci/roger/file_roger_art_provider.cpp build_and_run.ps1 docs/roger.md
git commit -m "Roger p2: overlay-truth capture mode (roger_truth_capture / -TruthCap)"
```

---

### Task 2: The cycle-diff backstop net

**Files:**
- Modify: `engines/sci/roger/file_roger_art_provider.h` (net members)
- Modify: `engines/sci/roger/file_roger_art_provider.cpp` (knob read; the net block in `snapshotNativeBaseline`; per-room reset)
- Modify: `docs/roger.md` (knob table row)

**Interfaces:**
- Consumes: `Roger::extractChangedBoxes` (unit-tested, Code facts), `markNativeDirty` (does clip/grow/map/defer — Code facts), `_inAnimateCycle` (already true at the seam), `_cycleLog`.
- Produces: `_diffNet` (default ON; ini `roger_diff_net`, env `ROGER_DIFF_NET` — the spec §6 escape hatch is `roger_diff_net=false`); the `ROGER-NET sum32=<ms> boxes=<n>` telemetry line under `-CycleLog`; members `_netPrevVisual`, `_netCurVisual`, `_haveNetPrev`, `_netCycleCount`, `_netCostAccumMs`. Task 3 relies on the heal property; Task 4 reads `ROGER-NET`.

- [ ] **Step 1: Add the members**

`file_roger_art_provider.h`, next to the Feeder B baseline members (`_nativeBaseline`, `_haveBaseline` — match their declaration style):

```cpp
	// ── Cycle-diff backstop net (spec Phase 2) ─────────────────────────────────
	bool _diffNet = false;               // roger_diff_net; default ON (escape hatch: =false)
	Common::Array<byte> _netPrevVisual;  // previous cycle's native visual buffer
	Common::Array<byte> _netCurVisual;   // this cycle's read (member: no per-cycle alloc)
	bool _haveNetPrev = false;           // false until the first cycle and after room change
	uint32 _netCycleCount = 0;           // 1-in-32 cost-log counter
	uint32 _netCostAccumMs = 0;          // summed ms over the last 32 cycles (ROGER-NET sum32)
```

- [ ] **Step 2: Read the knob (default ON)**

`file_roger_art_provider.cpp` constructor, after the `_truthCapture` read:

```cpp
	// Cycle-diff backstop net (spec Phase 2): at the snapshotNativeBaseline seam, diff
	// the native visual buffer against the previous cycle's copy and invalidate the
	// changed boxes — a native change that slipped past every invalidation hook heals
	// on the next cycle's present (brief flicker at worst, never persistent staleness).
	// Default ON; roger_diff_net=false is the runtime escape hatch (spec §6).
	const char *envNet = getenv("ROGER_DIFF_NET");
	_diffNet = envNet ? (Common::String(envNet) != "0" && Common::String(envNet) != "false")
	                  : (!ConfMan.hasKey("roger_diff_net") || ConfMan.getBool("roger_diff_net"));
```

- [ ] **Step 3: The net block in `snapshotNativeBaseline`**

Insert between `_inAnimateCycle = true;` and the existing Feeder B gate (`if (!_diffBackstop && ...) return;`) — the net must run even when Feeder B's snapshot is skipped:

```cpp
	// ── Cycle-diff backstop net (spec Phase 2) ──────────────────────────────
	// The visual buffer holds the WHOLE previous frame at this seam (see the
	// side-by-side comment below). Diff it against the previous cycle's copy and
	// mark the changed boxes dirty via markNativeDirty — which clips the status
	// strip, grows 1 native px, maps to overlay space, and (because _inAnimateCycle
	// is already set) routes to the scene seed union so the cycle-tail present
	// re-seeds clean background. Anything a hook missed heals here within one
	// cycle. Cost budget < 1.0 ms median: ROGER-NET sum32 under -CycleLog is the
	// measurement; the perf gate's busy+1ms threshold is the hard backstop.
	if (_diffNet && overlayShown() && g_sci && g_sci->_gfxScreen) {
		const uint32 netT0 = _cycleLog ? g_system->getMillis() : 0;
		GfxScreen *netScreen = g_sci->_gfxScreen;
		const int nw = netScreen->getWidth(), nh = netScreen->getHeight();
		_netCurVisual.resize((uint)nw * nh); // no-op after the first cycle
		for (int y = 0; y < nh; y++)
			for (int x = 0; x < nw; x++)
				_netCurVisual[(uint)y * nw + x] = netScreen->getVisual((int16)x, (int16)y);
		uint netBoxes = 0;
		if (_haveNetPrev && _netPrevVisual.size() == _netCurVisual.size()) {
			Common::Array<Common::Rect> changed;
			Roger::extractChangedBoxes(_netPrevVisual.begin(), _netCurVisual.begin(), nw, nh, changed);
			netBoxes = changed.size();
			for (uint i = 0; i < changed.size(); i++)
				markNativeDirty(changed[i]);
		}
		_netPrevVisual = _netCurVisual;
		_haveNetPrev = true;
		if (_cycleLog) {
			_netCostAccumMs += g_system->getMillis() - netT0;
			if ((++_netCycleCount & 31) == 0) {
				warning("ROGER-NET sum32=%ums boxes=%u", _netCostAccumMs, netBoxes);
				_netCostAccumMs = 0;
			}
		}
	}
```

- [ ] **Step 4: Reset the prev-cycle buffer on room change**

In `pushHiresBackground`, in the per-room reset block (where per-room capture state like `_staticSprites`/`_initCels` is cleared — locate it, don't guess):

```cpp
	_haveNetPrev = false; // room changed: don't diff across rooms (full present covers entry)
```

- [ ] **Step 5: Document the knob**

`docs/roger.md`, knob table, next to `roger_diff_backstop`:

```markdown
| `roger_diff_net` | on | Cycle-diff backstop net: each cycle, diff the native visual buffer against the previous cycle and invalidate changed regions — heals any missed invalidation within one cycle. Runtime escape hatch: set to `false` (env `ROGER_DIFF_NET=0` per-launch). Cost telemetry: `ROGER-NET sum32=<ms> boxes=<n>` under `-CycleLog` (budget: sum32 ≤ 32 ≈ 1 ms/cycle). Distinct from `roger_diff_backstop` (Feeder B *compositing* of unhooked draws); the net only *invalidates*. |
```

- [ ] **Step 6: Build + gate (net ON by default)**

Run: `.\build_and_run.ps1 -NoLaunch` then `powershell -File test\sci\roger\run-regression.ps1`
Expected: ALL PASS (33 checks), exit 0. Watch both perf entries hard — the net runs on the walking path every cycle; sq3 busy (baseline 4, limit 5) is the tight one. If busy is red with medians on baseline, re-run once (documented flake); if it is red twice, the net is over budget → measure `ROGER-NET sum32` (Step 7) before touching anything, then either optimize the read loop (e.g. hoist `getVisual`'s per-call math by reading row-by-row with x-loop only — still public API) or take the spec fallback: default `_diffNet` to false, record the finding in the Addendum, and continue (the escape-hatch default is an accepted spec outcome, not a failure).

- [ ] **Step 7: Measure the net cost**

```powershell
.\build_and_run.ps1 -NoBuild -Game qfg1 -SaveSlot 1 -Script test\sci\roger\scripts\qfg1-walk-perf.rin -TimeoutSec 90 -CycleLog
Select-String screenshots\roger-run.log -Pattern 'ROGER-NET sum32=(\d+)ms boxes=(\d+)' | ForEach-Object { $_.Matches[0].Groups[1].Value } |
    Measure-Object -Average -Maximum
```

Repeat for `-Game sq3-1 ... sq3-walk-perf.rin`. Requirement: average sum32 ≤ 32 (≈1 ms/cycle) for both games. Paste the numbers into the Addendum ("Task 2 net cost"). Over budget → the Step 6 fallback path (never an engine edit).

- [ ] **Step 8: Verify the escape hatch**

```powershell
$env:ROGER_DIFF_NET = "0"
powershell -File test\sci\roger\run-regression.ps1
Remove-Item Env:ROGER_DIFF_NET
```

Expected: ALL PASS — with the net off, behavior is exactly Task 1's (which was green). This proves `roger_diff_net=false` is a working rollback.

- [ ] **Step 9: Commit**

```bash
git add engines/sci/roger/file_roger_art_provider.h engines/sci/roger/file_roger_art_provider.cpp docs/roger.md
git commit -m "Roger p2: cycle-diff backstop net (roger_diff_net, default on)"
```

---

### Task 3: Heal validation — the net closes the fault class the marks used to carry alone

The spec's Phase 2 fault-injection criterion: "disable one legitimate invalidation source in a scratch build and confirm the net heals the region within one cycle (brief flicker acceptable, no persistent staleness)." Task 1 proved injection D is RED under truth captures *without* the net; this task proves the same injection is GREEN *with* it. Scratch edits only — never committed.

**Files:**
- Modify (scratch only, reverted): `engines/sci/roger/file_roger_art_provider.cpp`

**Interfaces:**
- Consumes: truth-capture mode (Task 1), the net (Task 2), the Phase 1 injection targets (Code facts).
- Produces: the heal-validation matrix Task 4 records in the spec addendum.

- [ ] **Step 1: Injection A (single source) + net + truth captures**

Scratch edit: comment out only `markNativeDirty(nativeRect);` in `onNativeEraseRect`. Then:

```powershell
.\build_and_run.ps1 -NoLaunch
$env:ROGER_TRUTH_CAPTURE = "1"
powershell -File test\sci\roger\run-regression.ps1
Remove-Item Env:ROGER_TRUTH_CAPTURE
```

Expected: **ALL PASS** — the net invalidates the erased region at the next cycle's seam, and every gate capture fires ≥ 1 cycle after the dismissal it checks (the `.rin` scripts' `wait` margins guarantee this). Record the result. If RED: the net is not healing — localize before editing (likely candidates: the net block sits after an early return it must precede; `_haveNetPrev` wrongly reset; boxes clipped away by the status-strip clip for a non-strip rect). Fix within Task 2's code, re-run Task 2's Steps 6–8, then re-run this step.

- [ ] **Step 2: Injection D (all three marks) + net + truth captures**

Scratch edit: additionally comment out the `markVacatedDirty(removedRects[i]);` loop body in `uiClearToken` and `markNativeDirty(screenRect);` in `onNativeShowRect` (same edits as Task 1 Step 9). Build; run the truth-capture gate exactly as Step 1.
Expected: **ALL PASS** — the strongest statement: with every §3.1 mark gone, the net alone keeps the presented frames correct at gate timescales. Record the result. (Contrast pair for the spec addendum: Task 1 Step 9 = D+truth **without** net = RED; this step = D+truth **with** net = GREEN.)

- [ ] **Step 3: Restore + clean rebuild + normal gate**

```powershell
git checkout -- engines/sci/roger/file_roger_art_provider.cpp
.\build_and_run.ps1 -NoLaunch
powershell -File test\sci\roger\run-regression.ps1
```

Expected: clean tree (`git status --short` shows nothing under `engines/`), ALL PASS.

- [ ] **Step 4: Record the matrix in this plan's Addendum**

Under "Heal-validation matrix": one row per run — {injection, net, truthCapture} → result, exact failing checks if any. Include the Task 1 Step 9 row for the contrast.

(No commit — this task produces evidence only; the Addendum is committed in Task 4.)

---

### Task 4: Perf + dirty-area evidence, twice-green exit, docs

**Files:**
- Modify: `docs/superpowers/plans/2026-07-03-roger-phase2-diff-net.md` (this file — Addendum)
- Modify: `docs/superpowers/specs/2026-07-02-roger-present-barrier-design.md` (Phase 2 result addendum)

**Interfaces:**
- Consumes: the complete Phase 2 implementation (Tasks 1–3) and their recorded numbers.
- Produces: the §8 exit evidence; the green light for the Phase 3 plan (which additionally requires the user's interactive soak of both games — spec §8).

- [ ] **Step 1: Dirty-area comparison vs Task 1 (totalAreaPerCycle)**

Re-run both walk-perf scripts with `-CycleLog` and compute the same statistics as Task 1 Step 8. Requirement: **totalAreaPerCycle ≤ Task 1 numbers × 1.10** for both games (medianArea recorded for context, not gated — Phase 1 lesson). Paste into the Addendum ("Task 4 dirty-area, net on"). If over: the net's boxes are not coalescing with the sprite-path rects — inspect `ROGER-PRESENT regions=` (should stay ~1–3 while walking) and `ROGER-NET boxes=` (walking should produce a handful of boxes hugging the moving sprite); a `regions` explosion means disjoint rects — fix by measurement (e.g. the grow(1) in `markNativeDirty` separating net boxes from sprite rects), never by dropping the net's marks.

- [ ] **Step 2: Twice-green exit run**

Run the full gate twice back-to-back on the clean build (net ON, truth capture OFF — the shipping configuration). Expected: ALL PASS (33 checks), exit 0, both runs. Flake allowance: busy-only red with medians on baseline → re-run that gate once; anything else = real regression → diagnose in the offending task's area and restart this step.

- [ ] **Step 3: Fill this plan's Addendum**

All bullets: Task 1 baselines + detection proof; Task 2 net cost (sum32 averages both games); Task 3 heal matrix; Task 4 dirty-area table; both twice-green tables.

- [ ] **Step 4: Record the Phase 2 result in the spec**

Append to `docs/superpowers/specs/2026-07-02-roger-present-barrier-design.md`:

```markdown
## Addendum: Phase 2 result (2026-07-XX)

Phase 2 shipped on branch `jon-p2-diffnet` (plan:
docs/superpowers/plans/2026-07-03-roger-phase2-diff-net.md — evidence tables in that
plan's addendum). Measured net cost: <sum32 averages, qfg1/sq3> (budget 32 ≈ 1 ms/cycle).
Overlay-truth capture mode (`roger_truth_capture`) shipped as the evidence channel;
under it, Phase 1's injection D turns the gate RED without the net — retroactively
satisfying Phase 1's fault-injection exit criterion — and GREEN with the net, proving
the heal property. <One line: any deviation — e.g. net defaulted off for cost, with
numbers.> Phase 3's mark-deletion list must be re-derived with truth captures per
injection (the Phase 1 "deletion candidates" claim was withdrawn as capture-masked).
```

Do NOT rewrite CLAUDE.md's invariants section — that is Phase 3 (spec §10).

- [ ] **Step 5: Commit + hand back**

```bash
git add -f docs/superpowers/plans/2026-07-03-roger-phase2-diff-net.md docs/superpowers/specs/2026-07-02-roger-present-barrier-design.md
git commit -m "Roger p2: phase exit - heal validation, net cost, twice-green gate"
```

Then report: Phase 2 complete on `jon-p2-diffnet`, ready to merge (human's call). Remind: the spec requires the user's interactive soak of BOTH games before Phase 3 begins — Phase 3 deletes bookkeeping, and the wiggle class proved scripted coverage alone misses interactive-only triggers.

---

## Amendment (2026-07-03, user-approved): at-dismissal stale-window scenario

Task 1's Step 9 stayed GREEN twice (scratch-source and grabOverlay truth captures) — see the
Detection-proof bullet below for the mechanism (`_dirtyPrev` prev-union heals the vacated
region on the very next present, stale window ~83 ms, gate settle waits 1000–2500 ms). The
user approved this amendment rather than accepting the analysis or pausing:

1. **Truth captures read the REAL overlay** (`g_system->grabOverlay`), not the scratch — the
   scratch-mirror premise in Code facts is circular (scratch == overlay only when invalidation
   is correct). Shipped in Task 1 (commit `94988037746`).
2. **New gate entry (at-dismissal stale window):** a `.rin` scenario that queues `capture`
   while a blocking dialog is up (cycle frozen, nothing consumes it) so the dismissal-fired
   present itself consumes the capture — landing the grab inside the stale window — plus a
   post-heal capture ~2 cycles later. Checks: pre-dialog baseline vs at-dismissal capture
   (SAME expected — this is the detection tripwire), baseline vs post-heal capture (SAME
   expected — the heal property). The entry runs with truth captures on (manifest gains a
   `truthCap` flag that passes `-TruthCap` through).
3. **Detection proof (amended Step 9):** under injection D + truth captures, the at-dismissal
   check must go RED (and post-settle checks stay green). That closes Phase 1's exit criterion.
4. **Task 3 expected-results amended:** under any injection WITH the net, the at-dismissal
   check stays RED — the net heals at the NEXT cycle's seam, i.e. after the dismissal present;
   the spec explicitly allows this flicker ("brief flicker acceptable, no persistent
   staleness"). Task 3's heal proof = every post-heal/post-settle check green under injection.
   The heal-validation matrix records the at-dismissal check's expected-red rows explicitly.
5. **Task 4 twice-green** runs the expanded gate (all entries incl. the new one) on the clean
   build — everything green, including the at-dismissal check (marks present ⇒ the vacated
   region is pushed at the dismissal present itself).

Standing finding for Phase 3 (record in the spec addendum): with all three §3.1 marks removed,
no persistent staleness exists at gate timescales even WITHOUT the net — the prev-union push
and compose-time dirtying carry the load one present later. The marks' unique value is the
at-dismissal present itself (intra-freeze correctness); the net's unique value is the
never-hooked class (no current gate scenario can make it stale; `ROGER-NET boxes` telemetry
evidences it operating).

### Resolution (2026-07-04, user-approved): detection/heal proofs unsatisfiable — ship net + findings

Amendment items 2–4 assumed the at-dismissal capture would go RED under injection. It does
not. Four experiments (scratch-source captures; grabOverlay captures; Diag-verified
at-dismissal capture consumed by the dismissal present; calibration with the three marks AND
`renderUiLayer`'s per-element dirty-adds all disabled) all stayed GREEN: post-Phase-1
invalidation is **layered** — the UI-layer `_dirtyPrev` self-perpetuating loop, the scene seed
union (including Phase 1's deferred-vacate cover), and compose-time dirtying each
independently reseed a vacated region. No scriptable fault produces observable staleness at
any reachable capture point. Additionally: the net is structurally blind to blocking-dialog
dismissals (zero `kernelAnimate` ticks while frozen ⇒ the native buffer round-trips
identically across the dialog episode ⇒ the cycle diff sees nothing).

User-approved final shape:
- **Task 2 proceeds unchanged** — the net ships as insurance against future invalidation
  holes (cost-gated per spec; escape hatch `roger_diff_net=false`).
- **Task 3 becomes the documented redundancy matrix**: the injection runs already performed,
  plus injection runs WITH the net for completeness (expected green; the rows record why).
- **Task 4 unchanged** (perf, dirty-area vs Task 1 baseline, twice-green exit).
- The at-dismissal manifest entry is **NOT added** (zero detection power — it passes under
  total dirty-source removal); `qfg1-atdismiss.rin` is committed as a research artifact with
  a header comment saying so. The `truthCap` manifest flag and grabOverlay truth captures
  stay (correct evidence channel; the masking was real and is now fixed — the greens above
  are genuine architecture redundancy, not capture masking).
- Spec addendum must record: the detection criterion is unsatisfiable because Phase 1's
  architecture is redundantly self-healing (its strongest possible validation); the net's
  freeze-bracket blind spot; the implications for Phase 3's deletion list (deletions are
  lower-risk than feared, but the wiggle-class caveat about interactive-only triggers still
  stands, and the marks remain load-bearing ONLY as the first layer of a redundant stack).

## Addendum: Phase 2 exit evidence

(Filled during execution.)

- **Task 1 dirty-area baseline (pre-net):**
  - qfg1: `presents=186 cycles=176 medianArea=868414 totalAreaPerCycle=940672`
  - sq3:  `presents=193 cycles=181 medianArea=390062 totalAreaPerCycle=586131`
- **Detection proof (injection D + truth captures + grabOverlay fix, no net):** STAYED GREEN (both scratch-source and grabOverlay approaches) — ALL 33 checks passed both times. Root cause (deepened by second implementer): the overlay also self-heals within 1 cycle via `_dirtyPrev`. When a dialog is rendered, `renderUiLayer` calls `addDirtyRect` for each UI element (dialog rects → `_dirtyCur`). After `rollPresentDirty`, `_dirtyCur` → `_dirtyPrev`. In the NEXT `renderScene`, `_dirtyPrev` is included in the seed union AND in `dirtyUnion` for `presentToOverlay` — so the dialog region is pushed by the very next `kernelAnimate` cycle, regardless of whether `markVacatedDirty`/`markNativeDirty` fired. The stale window (between the dismiss-fired `presentWithUi` and the next `renderFrame`) is shorter than one game cycle (~83ms). All gate scenarios wait 1000-2500ms post-dismiss; the overlay is healed long before any capture fires. NEW SCENARIO NEEDED: `capture` command issued BEFORE the dismiss action (making it pending when the dismiss-fired `presentWithUi` runs), so the grab lands during the ~83ms stale window. No existing gate scenario does this. Human scoping decision required. **AT-DISMISSAL SCENARIO ATTEMPTED AND BLOCKED (2026-07-04):** New scenario `qfg1-atdismiss` (QFG1 room 320, dialog dismiss with `capture atdismiss` pending at dismiss time, grabOverlay truth capture) was built and tested. Diag confirmed the capture IS consumed by the dismiss `presentWithUi` bounded path. However, the check still STAYED GREEN under injection D, injection A, and all combinations. Root cause: `renderUiLayer`'s `addDirtyRect` calls create a **self-perpetuating loop** — each dialog-rendering present adds the full dialog extent to `_dirtyCur`, which becomes `_dirtyPrev` for the next present, which includes it in `dirtyUnion`, which re-renders the dialog (it's still in `_uiLayer`), which adds it again to `_dirtyCur`. At dismiss time, `_dirtyPrev` ALWAYS contains the full dialog extent from the last rendering present. `patchCompositeRegions` seeds the dialog area clean from `_sceneCache`, regardless of `markVacatedDirty`. The `markVacatedDirty`/`markNativeDirty` marks are REDUNDANT for the dialog-dismiss path — their unique value is for non-UI-layer native draws (bitsRestore on non-UI regions) where no `renderUiLayer` addDirtyRect provides coverage. Such scenarios are not easily scriptable via `.rin`. INFRASTRUCTURE COMMITTED: `run-regression.ps1` gains per-entry `truthCap` flag (commit `3e5228d8979`). Script `test/sci/roger/scripts/qfg1-atdismiss.rin` exists as research artifact (untracked). Manifest entry NOT committed (detection tripwire doesn't trigger). DETECTION PROOF REMAINS BLOCKED.
- **Task 2 net cost:** qfg1: avg sum32 ~5.4 ms, max ~11 ms; sq3: avg sum32 ~5.0 ms, max ~7 ms (both well under budget avg ≤ 32 / sum32 ≤ 32). Measured at Task 2 ship time. Re-confirmed in Task 4 dirty-area runs: qfg1 avg 7.2 ms / sq3 avg 5.6 ms (ambient load variation; still well under budget). Net defaults ON.
- **Heal-validation matrix:**

  | Run | Injection | Net | TruthCapture | Result | Reason |
  |-----|-----------|-----|--------------|--------|--------|
  | T1-S9a | D (all 3 marks) | OFF | scratch-source (pre-grabOverlay) | GREEN (33/33) | Capture-masked: needFullSource forced full clean recompose, bypassing bounded path |
  | T1-S9b | D (all 3 marks) | OFF | grabOverlay | GREEN (33/33) | Layered redundancy: _dirtyPrev self-perpetuating loop from renderUiLayer addDirtyRect heals the vacated region before any post-settle capture fires |
  | T1-atdismiss-clean-1 | none | OFF | grabOverlay | GREEN (33/33) | Clean build; at-dismissal capture + post-heal capture both correct |
  | T1-atdismiss-clean-2 | none | OFF | grabOverlay | GREEN (33/33) | Second clean run confirms baseline correctness |
  | T1-atdismiss-injA | A (onNativeEraseRect only) | OFF | grabOverlay | GREEN (33/33) | _dirtyPrev loop covers bitsRestore sub-rect; stale window < 83ms, shorter than gate settle wait |
  | T1-atdismiss-injD | D (all 3 marks) | OFF | grabOverlay | GREEN (33/33) | Same mechanism as T1-S9b; all four dirty-source paths (marks + renderUiLayer addDirtyRect) removed; scene-seed union from renderFrame still heals within one cycle |
  | T1-calibration | D + renderUiLayer addDirtyRect disabled | OFF | grabOverlay | GREEN (33/33) | renderFrame→renderScene always heals within one cycle (~83ms) via scene-seed from sprite movement; stale window shorter than .rin script timing resolution |
  | T3-S1 | A (onNativeEraseRect only) | ON | grabOverlay | GREEN (33/33) | Net covers injection A: diff sees the bitsRestore-erased region and calls markNativeDirty per box; within-cycle invalidation; also redundantly covered by _dirtyPrev loop |
  | T3-S2 | D (all 3 marks) | ON | grabOverlay | GREEN (33/33) | Net covers injection D: diff sees all changed native boxes and markNativeDirty per box; additionally the blocking-dialog class is blind to the net (frozen cycle → native buffer identical across dialog episode → zero diff boxes) but _dirtyPrev loop redundantly covers it |
  | T3-S3-clean | none | ON | OFF | 32/33 FAIL: sq3-walk-perf busy=6/4 (×2), busy=21/17 qfg1 run3 | System-load noise: medians on baseline (83/83, 84/84) for all runs; documented busy-only flake; code is byte-identical to injection builds which passed busy. Flake resolved by T4 re-measurement after cooldown (below). |
  | T3-S3-flake-reconfirm | none | ON | OFF | GREEN (33/33) | Controller re-measured after cooldown (parallel session idle): busyMedian=5 periodMedian=83 p90=84 cycles=181 → PASS. T4 twice-green runs both confirmed sq3 busy=5/4, qfg1 busy=17/17. Transient system load during T3-S3-clean; not a code regression. |

- **Task 4 dirty-area (net on):**
  - qfg1: `presents=187 cycles=176 medianArea=850137 totalAreaPerCycle=927044` — gate: 927044 ≤ 1,034,739 (Task1×1.10) PASS
  - sq3:  `presents=194 cycles=181 medianArea=378185 totalAreaPerCycle=576665` — gate: 576665 ≤ 709,218 (Task1×1.10) PASS
  - Net boxes stay ~0-75/cycle while walking (boxes is last-cycle count, not 32-cycle sum); dirty-area not inflated vs Task 1 baseline.
- **Twice-green gate tables:**

  **Run 1 (2026-07-04, net ON, truth capture OFF):**

  | Entry | Check | Result | Detail |
  |-------|-------|--------|--------|
  | qfg1-smoke | run | PASS | exit 0 |
  | qfg1-smoke | scriptWarn | PASS | |
  | qfg1-smoke | exists:boot | PASS | |
  | qfg1-smoke | exists:after-walk | PASS | |
  | qfg1-smoke | exists:after-arrow | PASS | |
  | qfg1-smoke | exists:typed | PASS | |
  | qfg1-smoke | exists:look-dialog | PASS | |
  | qfg1-smoke | exists:dismissed | PASS | |
  | qfg1-cmdbox | run | PASS | exit 0 |
  | qfg1-cmdbox | scriptWarn | PASS | |
  | qfg1-cmdbox | same:before~after | PASS | |
  | sq3-dismiss-matrix | run | PASS | exit 0 |
  | sq3-dismiss-matrix | scriptWarn | PASS | |
  | sq3-dismiss-matrix | same:m0~esc1 | PASS | |
  | sq3-dismiss-matrix | same:m0~esc2 | PASS | |
  | sq3-dismiss-matrix | same:m0~clk1 | PASS | |
  | sq3-dismiss-matrix | same:m0~emp1 | PASS | |
  | sq3-wiggle | run | PASS | exit 0 |
  | sq3-wiggle | scriptWarn | PASS | |
  | sq3-wiggle | same:w0~w1 | PASS | |
  | sq3-wiggle | same:w0~w2 | PASS | |
  | qfg1-dialog-cycle | run | PASS | exit 0 |
  | qfg1-dialog-cycle | scriptWarn | PASS | |
  | qfg1-dialog-cycle | presence:d1 | PASS | 20000 px (>= 20000) |
  | qfg1-dialog-cycle | presence:d2 | PASS | 20000 px (>= 20000) |
  | qfg1-dialog-cycle | same:g0~g1 | PASS | |
  | qfg1-dialog-cycle | same:g0~g2 | PASS | |
  | qfg1-walk-perf | run | PASS | exit 0 |
  | qfg1-walk-perf | scriptWarn | PASS | |
  | qfg1-walk-perf | perf | PASS | median=83/83 p90=84/84 busy=17/17 n=167 |
  | sq3-walk-perf | run | PASS | exit 0 |
  | sq3-walk-perf | scriptWarn | PASS | |
  | sq3-walk-perf | perf | PASS | median=83/83 p90=84/84 busy=5/4 n=171 |

  ALL PASS (33 checks)

  **Run 2 (2026-07-04, net ON, truth capture OFF):**

  | Entry | Check | Result | Detail |
  |-------|-------|--------|--------|
  | qfg1-smoke | run | PASS | exit 0 |
  | qfg1-smoke | scriptWarn | PASS | |
  | qfg1-smoke | exists:boot | PASS | |
  | qfg1-smoke | exists:after-walk | PASS | |
  | qfg1-smoke | exists:after-arrow | PASS | |
  | qfg1-smoke | exists:typed | PASS | |
  | qfg1-smoke | exists:look-dialog | PASS | |
  | qfg1-smoke | exists:dismissed | PASS | |
  | qfg1-cmdbox | run | PASS | exit 0 |
  | qfg1-cmdbox | scriptWarn | PASS | |
  | qfg1-cmdbox | same:before~after | PASS | |
  | sq3-dismiss-matrix | run | PASS | exit 0 |
  | sq3-dismiss-matrix | scriptWarn | PASS | |
  | sq3-dismiss-matrix | same:m0~esc1 | PASS | |
  | sq3-dismiss-matrix | same:m0~esc2 | PASS | |
  | sq3-dismiss-matrix | same:m0~clk1 | PASS | |
  | sq3-dismiss-matrix | same:m0~emp1 | PASS | |
  | sq3-wiggle | run | PASS | exit 0 |
  | sq3-wiggle | scriptWarn | PASS | |
  | sq3-wiggle | same:w0~w1 | PASS | |
  | sq3-wiggle | same:w0~w2 | PASS | |
  | qfg1-dialog-cycle | run | PASS | exit 0 |
  | qfg1-dialog-cycle | scriptWarn | PASS | |
  | qfg1-dialog-cycle | presence:d1 | PASS | 20000 px (>= 20000) |
  | qfg1-dialog-cycle | presence:d2 | PASS | 20000 px (>= 20000) |
  | qfg1-dialog-cycle | same:g0~g1 | PASS | |
  | qfg1-dialog-cycle | same:g0~g2 | PASS | |
  | qfg1-walk-perf | run | PASS | exit 0 |
  | qfg1-walk-perf | scriptWarn | PASS | |
  | qfg1-walk-perf | perf | PASS | median=83/83 p90=84/84 busy=17/17 n=166 |
  | sq3-walk-perf | run | PASS | exit 0 |
  | sq3-walk-perf | scriptWarn | PASS | |
  | sq3-walk-perf | perf | PASS | median=83/83 p90=84/84 busy=5/4 n=171 |

  ALL PASS (33 checks)
