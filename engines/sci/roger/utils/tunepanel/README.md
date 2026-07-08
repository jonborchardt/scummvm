# Tune Panel — the in-game quick-tune debug dialog (F12)

A quarantined dev utility: a mouse-driven, in-game debug dialog for tuning the
OMYAC enhance-pass sequence and the view-scaler module while actually playing.
Toggle it with **F12** (or **Ctrl+Shift+T**) in Enhanced display mode — a fixed
90×184 game-space panel docks on the right (a `<`/`>` title button flips it to
the left edge).

It is **session-only**: nothing it does is written to `scummvm.ini` or the
generation disk cache. Applying off-config passes forces `kGenMemory`
(regenerate in RAM, cache untouched); returning to the config passes restores
the prior gen mode. Close the panel or restart, and the config wins again.

Spec: `docs/superpowers/specs/2026-07-05-roger-tune-panel-design.md`. Built for
the 2026 MMPX view-scaler judging (concluded 2026-07-06 — the shipping 6x won);
kept afterwards as the standing pass-tuning tool.

---

## The panel

- **Variant rows** (top): one per registered view-scaler module
  (`roger_view_scaler.h` registry). A single module — the shipping 6x — is
  registered today, so exactly one row shows; clicking a row applies that
  scaler immediately (non-zero registry indices bypass the disk cache).
- **Known-good preset rows** (below the variants): one per `goodPassPattern()`
  entry in `roger_passes.cpp` — the curated, best-first registry the picker's
  suggestion buttons and the Eye Exam seed share. Labels are the compact
  strings (provenance notes live in the registry; no room for tooltips here).
  Clicking a row stages **and applies** that pattern in one click — the
  easy-swap path — and the row stays lit while the staged list matches it.
  Session-only like everything else here (regen in memory; the ini is never
  written).
- **Pass chips**: the staged OMYAC pass sequence, one chip per pass
  (`f`/`l`/`a`), 7 per row. Click a chip to select it, then use the ops row:
  `x` delete, `<`/`>` move, `+f`/`+l`/`+a` insert after the selection.
- **clear / reset / apply** (bottom-anchored): clear empties the staged list,
  reset restores the applied list, **apply** regenerates the current scene with
  the staged passes (highlights while edits are pending). Chip edits do
  NOTHING until Apply — that's the point.
- **Status line** (bottom): compact pass stamp + `*` pending marker + last
  Apply's regen wall-clock, e.g. `ffl * 812ms`.

All layout and hit-testing are in **game space (320×200)** with a fixed panel
rect and bottom-anchored button rows, so `.rin` automation clicks stay valid at
any window size. The right-side geometry is LOCKED by
`test/sci/roger/test_tune_panel.h` (`test_script_geometry_lock`) and
`test/sci/roger/scripts/tune-panel-smoke.rin` — moving a widget breaks both on
purpose.

## How it's wired (and why that's the whole surface)

This folder holds only **pure, engine-free logic**: panel state, widget layout,
hit-test geometry, and drawing into an RGBA scene surface. It unit-tests
without an engine (same isolation as the Studio's pure helpers in
`utils/studio/roger_studio_render.h`).

The glue lives in `file_roger_art_provider.{h,cpp}`: the provider owns a
`TunePanelState`, draws the panel at the end of its composite pass, and
implements `toggleTunePanel()` / `tunePanelMouse()`. `event.cpp` routes F12 and
mouse events through those **abstract provider virtuals**
(`roger_art_provider.h`) — it never sees a tunepanel type.

## Quarantine contract

Nothing in the engine may depend on `utils/tunepanel/` except the documented
glue. The only permitted references:

- the F12 integration block in `engines/sci/roger/file_roger_art_provider.{h,cpp}`
  (state member + draw/toggle/mouse glue),
- the `engines/sci/module.mk` object list,
- `build_tests.ps1`'s test registration
  (`test/sci/roger/test_tune_panel.h` covers this pure module).

Everything else — including `event.cpp`'s key/mouse routing — must go through
the plain virtuals on the abstract provider, which name no tunepanel types.
This module may only consume stable SCI-free roger seams
(`roger_widgets.h`, `roger_passes.h`, `roger_view_scaler.h`,
`roger_coords.h`) — never provider/compositor internals, never SCI engine
state, and never anything under `utils/studio/`. It never writes the
ini or the generation disk cache, and with the panel closed the render path
is untouched (the draw call is gated on `open && kModeEnhanced`).
