# Roger Studio — the standalone omyac tuning environment

A quarantined dev utility: a mouse-driven, single-scene laboratory for tuning
the omyac enhance pipeline. It shows one scene the way the game would draw it
(hires plate + a view cel composited on top), holds **two independent setting
slots (A and B)**, and lets you flip, split, and diff them until you can see
exactly what a parameter or pass change does. Everything is generated in RAM —
the generation disk cache is never read or written.

Specs: `docs/superpowers/specs/2026-07-02-roger-studio-design.md` (v1) and
`2026-07-02-roger-studio-v2-ui-design.md` (the shipping v2 UI).

---

## Launching

```powershell
.\build_and_run.ps1 -Studio              # SQ3 scene pool (default)
.\build_and_run.ps1 -Studio -Game qfg1   # QFG1 resources
```

`-Studio` sets the `ROGER_STUDIO` env var for that launch only; the hook in
`sci.cpp` starts the Studio instead of the game (resources and graphics are
alive, no game scripts run) and the process exits when you quit. One game per
run — the Studio can only render the resources of the game it booted.

Startup defaults: SQ3 opens pic 2 with view 12 loop 1 (the classic street +
Roger); any other game opens its first pic/view. Both slots start from the
**ini-effective pass list** (`roger_omyac_passes`, or the built-in default
when unset) — "Reset" always returns to that.

## The screen

Top = the **scene area**; bottom 560 overlay px = the **control panel**. The
pointer is a composited crosshair (the hardware cursor is invisible over the
overlay). Only two keys exist, for automation convenience: **Esc** quits,
**E** exports; everything else is mouse. The control panel is painted at full
display resolution through the shared kit's PanelPainter (TTF labels, PanelStyle
palette); the widgets keep their half-res layout space and the /2 hit-test
mapping unchanged.

### Scene area interaction

- **Left-click** places the cel (bottom-centre anchor at the click, in native
  320×190 coords) and starts a drag; the `@(x,y)` readout in the panel tracks
  it. Cel moves reuse the cached plate — no omyac regen at drag rate.
- **Right-drag** pans; **mouse wheel** zooms (0.5×fit up to 8×, anchored at
  the cursor); **Fit** re-centres the plate to the window.
- **grid: on** overlays a light line at every plate-pixel boundary once zoom
  ≥ 3× — for judging single-pixel placement.

### The two slots (A / B)

Each slot independently holds:

- a **`pic enhance:` mode** — one toggle that cycles the shared pass-mode
  list (seeded from the curated `goodPassPattern()` registry + the
  ini-effective passes) **plus a trailing `nearest`** (the nearest-neighbour
  reference plate, the "before" picture — the old separate `plate:` toggle is
  folded in here). Each click applies immediately; the generator's in-memory
  cache makes a revisited mode a copy instead of a fresh omyac run.
- a **`view enhance:` mode** — one toggle that cycles the `roger_view_scaler.h`
  registry (the shipping 6x today) **plus a trailing `nearest`** (plain
  blocky upscale) for the cel. Cel-tier: cycling never regenerates the plate.
- the **7 registered OmyacParams** (vote thresholds, endpoint rule, isolated
  pixel pass, tie-break blend, diagonal flank suppression — each row has
  `-`/`+` or an on/off toggle, with a plain-English hover help line),

`edit: A B` picks which slot the toggles/params edit. **Copy A>B** clones
A's settings into B — set up a baseline in A, copy, then nudge B.

### The pass builder (build row)

Same linear builder as the F12 tune panel: display-only chips show the
sequence being built; **`+f` `+l` `+a`** append a pass, **`clear`** empties it
(add → wireframe), and **`add`** registers the built sequence as a new
`pic enhance:` mode, selects it in the active slot, and applies. `add`
highlights while it would change the active slot. The old caret/selection
editing (`x`, `<`, `>`, Reset) is gone — the ini-effective passes are always
one of the cycle's modes, which replaces Reset.

### Display modes

| Button | Shows |
|---|---|
| **Show A** / **Show B** | one slot full-frame |
| **Split** | A left, B right, shared pan/zoom, stamped labels |
| **Diff** | per-pixel white-on-black difference of A vs B, plus an automatic **alignment readout** — a ±3 px SAD search prints `best align: dx=… dy=…` (and logs `ROGER-STUDIO diff offset`), the sub-pixel-shift detector |
| **Grid** | one tile per view-enhance mode — every registered scaler module plus the trailing `nearest` — the current cel through each at a shared native footprint (with one scaler registered this is the 6x-vs-nearest comparison) |

Diff/SAD rebuilds are skipped while dragging the cel (they'd stall the drag)
and rebuilt on release.

### Scene toggles and playback

- **pink: on** — recolours every plate pixel that nothing official painted
  (the `fillNullPixels` backfill) hot pink. Diagnostic for "how much of this
  plate is guesswork". Nearest-ref plates carry no mask, so no pink there.
- **view: on/off** — include or drop the cel.
- **play / spd- / spd+** — cycles the cel through its loop
  (300/200/150/100/66 ms per cel) in every display mode, for judging a scaler
  on a walking animation rather than a frozen pose.

### Export (E key or the Export PNG button)

Writes a stamped PNG to `screenshotpath` (default `screenshots/`); the
filename encodes what you were looking at, so exports are self-documenting
evidence:

| Mode | Filename |
|---|---|
| Show A/B | `studio-scene<pic>-A-<params>-<passes>[-nref][-scaler].png` |
| Split | `studio-scene<pic>-AB-<stampA>-vs-<stampB>.png` |
| Diff | `studio-scene<pic>-diff-<stampA>-vs-<stampB>.png` |
| Grid | `studio-grid<view>-l<loop>-c<cel>.png` (fixed 8 px per native px, window-independent) |

Note for automation: the Studio never enters the game loop, so `.rin` input
scripts and their `snap`/`capture` do **not** drive it — use the E export (or
a desktop-level screen grab) for captures.

## Regen cost model (why it feels fast)

Three invalidation tiers, cheapest first: display-only (pan/zoom/pixel-grid —
redraw), **cel-only** (place/drag/loop/cel/view/pink/**view enhance** —
recomposite over the cached plate), and **plate** (pic/params/**pic enhance** —
full omyac regen, the `<n>ms` readout in the status line). Only the last one
is expensive, and the generator's in-memory cache (kGenMemory, bounded FIFO)
takes the sting out of it: cycling `pic enhance:` back to a mode you've
already rendered for this pic+params is a surface copy, not a regen.

## Quarantine contract

Nothing in the engine may depend on `utils/studio/`. The only permitted
references: the env-gated `ROGER_STUDIO` hook in `sci.cpp`, the
`engines/sci/module.mk` object list, `build_tests.ps1`'s source + test
registration, and the unit tests (`test/sci/roger/test_studio_render.h`,
`test_shift_lock.h` — they cover the pure helpers in
`roger_studio_render.{h,cpp}`). Production code must never include anything
from this folder — the generic widget primitives it shares with the F12 tune
panel live in the neutral `sci/roger/ui/roger_widgets.h`, and the pass-list edit
ops/stamps in `sci/roger/gen/roger_passes.h`, precisely so nothing outside
`utils/` ever needs a studio header.

This module may only consume stable roger seams (`roger_asset_gen.h`,
`roger_view_scaler.h`, `roger_passes.h`, `ui/roger_widgets.h`,
`ui/roger_panel_style.h`, `png_loader.h`)
— never provider/compositor internals. It never touches the generation disk
cache (its own `RogerAssetGen` runs `kGenMemory` with an empty cache dir),
never writes `scummvm.ini`, and a launch without the env var is
byte-identical to one where this folder doesn't exist.
