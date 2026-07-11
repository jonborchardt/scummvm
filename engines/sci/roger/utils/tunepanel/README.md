# Tune Panel — the in-game quick-tune debug dialog (F12)

A quarantined dev utility: a mouse-driven, in-game debug dialog for tuning the
OMYAC enhance-pass sequence and the view-scaler module while actually playing.
Toggle it with **F12** in Enhanced display mode — a fixed
90×184 game-space panel docks on the right (a `<`/`>` title button flips it to
the left edge).

It is **session-only**: nothing it does is written to `scummvm.ini` or the
generation disk cache. Applying off-config passes forces `kGenMemory`
(regenerate in RAM, cache untouched); returning to the config passes restores
the prior gen mode. Closing the panel does NOT revert — applied settings stay
live for the session; restart (or cycle back to the config passes) to get the
config again.

The view-scaler judging concluded (6x won); this panel is the standing
pass-tuning tool.

---

## The panel

- **`log:` toggle** (top): mirrors the F11 per-frame Roger diagnostic-log
  toggle as a clickable row (lights while on).
- **`view enhance:` toggle**: one row that cycles the view-scaler modes and
  applies immediately. The known modes are the `roger_view_scaler.h` registry
  scalers (the shipping **6x (s2>s3)** is the only one today) plus a synthetic
  **nearest** — a plain no-enhancement upscale, the blocky "before" for an A/B
  comparison (cache-bypassed by the generator). More scalers registering just
  adds more stops to the cycle.
- **`pic enhance:` toggle**: one row that cycles the available OMYAC pass modes
  **plus a trailing `nearest`** (the zero-enhancement native plate replicated
  ×6 — the "before" for plate A/B) and applies on click; the label shows the
  selected mode's compact string (e.g. `ffffffflff`) or `nearest`. The list is
  seeded from the `goodPassPattern()` registry in `roger_passes.cpp` (curated,
  best-first — shared with the picker's suggestion buttons and the Eye Exam
  seed) and grows via **add** (below). Cycling is cheap to revisit: the
  generator memory-caches plates + priority maps per mode (bounded FIFO,
  kGenMemory only), so returning to a computed mode is a copy, not a regen.
- **Pass chips**: DISPLAY-ONLY view of the sequence being built, one dim chip
  per pass (`f`/`l`/`a`), 7 per row. Not clickable.
- **Build row** (bottom-anchored, one line): `+f` `+l` `+a` append a pass to the
  built sequence, `clear` empties it, **`add`** registers the built sequence as
  a new `pic enhance:` mode, selects it, and applies (regenerates the scene).
  `add` highlights while there are edits to commit. The built sequence does
  NOTHING to the scene until **add** — that's the point.
- **Status line** (bottom): compact pass stamp of the built sequence + `*`
  pending marker + last apply's regen wall-clock, e.g. `ffl * 812ms`.

Everything is **session-only**: cycling `pic enhance:` off-config (including
`nearest`) or adding a custom mode regenerates in memory (`kGenMemory`);
returning to the config passes restores the prior gen mode. The ini and the
generation disk cache are never written. `view enhance: nearest` and any
non-zero view-scaler index likewise bypass the disk cache.

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
mouse events through those **fork-only provider methods**
(`file_roger_art_provider.h`, via the neutral `interceptEvent` seam → `FileRogerArtProvider::interceptEvent` → `toggleTunePanel`/`tunePanelMouse`) — it never sees a tunepanel type.

## Quarantine contract

Nothing in the engine may depend on `utils/tunepanel/` except the documented
glue. The only permitted references:

- the F12 integration block in `engines/sci/roger/file_roger_art_provider.{h,cpp}`
  (state member + draw/toggle/mouse glue),
- the `engines/sci/module.mk` object list,
- `build_tests.ps1`'s test registration
  (`test/sci/roger/test_tune_panel.h` covers this pure module).

Everything else — including `event.cpp`'s key/mouse routing — must go through
the plain virtuals on the neutral `SciGfxObserver` seam, which name no tunepanel types.
Panel visuals (colors, fonts, layout painting) use the shared
`ui/roger_panel_style.h` kit (PanelStyle + PanelFonts + PanelPainter);
the provider bakes the rendered surface into `_tuneBake` and re-renders it
only when the signature changes (state, hover, or dest-rect/scale);
repeated presents at steady state are a surface copy, not a repaint.
This module may only consume stable SCI-free roger seams
(`ui/roger_widgets.h`, `ui/roger_panel_style.h`, `roger_passes.h`,
`roger_view_scaler.h`, `roger_coords.h`) — never provider/compositor
internals, never SCI engine state, and never anything under `utils/studio/`.
It never writes the ini or the generation disk cache, and with the panel
closed the render path is untouched (the draw call is gated on
`open && kModeEnhanced`).
