# Roger — high-resolution art enhancement for SCI0 EGA games

Roger displays **high-resolution art** for Sierra SCI0 EGA adventures — Space
Quest III and Quest for Glory I (EGA) — while leaving the original game logic
completely intact. The game still runs at its native 320×200 resolution
(events, pathfinding, priority/control buffers, scripts); only the **display**
is high-resolution, presented through ScummVM's OSystem overlay.

All art is **generated in-engine from the game's own SCI resources** (the
"omyac" upscaling pipeline). No pre-made art packs are downloaded or consumed
from disk — you only need the original game data files, exactly as for any
other game ScummVM runs.

Roger is a display-layer provider **inside the existing SCI engine** — it is
not a separate engine, adds no game detection, and when disabled the SCI
engine behaves byte-identically to stock ScummVM.

## Supported games

| Game | Status |
|------|--------|
| Space Quest III | Supported, validated |
| Quest for Glory I (EGA) | Supported, validated |
| Other SCI0 EGA titles | Untested; may work |
| VGA / SCI1+ titles | **Out of scope by design** — rejected at startup |

EGA SCI0 only is a permanent design decision, not a deferred feature.

## Quickstart (Windows)

A PowerShell script at the repository root installs dependencies (vcpkg),
generates the Visual Studio solution, builds, and launches:

```powershell
.\build_and_run.ps1
```

At startup the **Roger launcher** lists your configured SCI games with their
cache status and per-game settings (pre-cache scope, enhancement passes,
font, cursor). Select a game and click **Launch**. On first launch the art
cache is generated automatically; later launches load from the cache.

In-game:

- **F10** (or Ctrl+Shift+U) cycles display modes:
  **Enhanced** → **Original** (native 320×200) → **Side-by-Side** comparison.
- **Ctrl+Shift+F** cycles the dialog/body font through a shortlist.
- **Ctrl+Shift+[ ] / ; '** tune the enhancement passes live.

See the [user documentation](../../../docs/roger.md) for the launcher,
display modes, and every configuration knob.

## What's enhanced

- Room backgrounds (the "plate"), upscaled and edge-enhanced
- Ego, props, and inventory images (upscaled native cels)
- Dialog windows, buttons, text-input fields, menus, and the status banner
- Screen transitions (fade/dissolve) and live palette effects
- The mouse cursor (a composited high-resolution arrow)

Walkability, occlusion, and all game behavior ride SCI's own native buffers,
so gameplay is unchanged.

## Configuration

All settings are optional `scummvm.ini` keys; the most useful:

| Key | Default | Meaning |
|-----|---------|---------|
| `roger_gen_mode` | `cache` | art generation mode; `prebuilt` is the off-switch (pure native rendering) |
| `roger_display_mode` | `enhanced` | startup display mode: `enhanced`, `original`, `sbs` |
| `roger_ui_font` | `GoMono-Regular.ttf` | dialog/body font (from ScummVM's bundled `fonts.dat`) |
| `roger_precache` | `off` | generate all art up front instead of on demand |

The full table (25+ knobs, including debugging and automation switches) is in
[docs/roger.md](../../../docs/roger.md#config-knobs).

## Generated cache

Generated art is cached as content-keyed PNGs in a sibling of the game
directory (e.g. `sq3-roger/cache/` next to `sq3/`). The cache is safe to
delete at any time — it is regenerated on demand — and stale entries are
invalidated automatically when the generation pipeline changes.

## Documentation

- [docs/roger.md](../../../docs/roger.md) — user guide: launcher, display
  modes, config knobs, cache layout, input automation, Roger Studio
- [docs/roger/](../../../docs/roger/) — fork-maintenance and upstreaming
  documents (in progress)
- [CLAUDE.md](../../../CLAUDE.md) — developer orientation: rendering
  invariants, hook inventory, performance discipline, build workflows

## Examples

- **Input scripts** (`.rin`) — headless scripted-input runs for regression
  testing and captures: [test/sci/roger/scripts/](../../../test/sci/roger/scripts/)
  (e.g. `qfg1-smoke.rin`; grammar in [docs/roger.md](../../../docs/roger.md#rin-grammar))
- **Roger Studio** — a mouse-driven tuning environment for A/B-comparing
  enhancement settings on a live scene: `.\build_and_run.ps1 -Studio`
- **Regression suite** — `test/sci/roger/run-regression.ps1`
  (manifest-driven phase-gate checks)

## Source layout

| Area | Files |
|------|-------|
| Provider interface + wiring | `roger_art_provider.h`, `file_roger_art_provider.{h,cpp}` |
| Art generation (omyac pipeline) | `roger_asset_gen.*`, `roger_pic_parser.*`, `roger_pic_native.*`, `roger_omyac.*`, `roger_scale.*`, `roger_ega_blend.*` |
| Compositing & presentation | `roger_compositor.*`, `roger_ui_layer.*`, `roger_text.*`, `roger_cursor.*`, `roger_effects.*` |
| Caching | `view_cache.*`, `png_loader.*` |
| Launcher | `roger_launcher.*`, `roger_launcher_dialog.*` |
| Input automation | `roger_input.*` (engine-agnostic; no SCI includes) |
| Tuning environment | `roger_studio.*`, `roger_studio_render.*` |
| Validation | `roger_selftest.*`, `roger_capabilities.*` |

Hook sites in the SCI engine proper (`engines/sci/graphics/`) are mechanical,
null-guarded calls through the abstract provider — see the hook table in
[CLAUDE.md](../../../CLAUDE.md).

Unit tests live in [test/sci/roger/](../../../test/sci/roger/) (CxxTest);
on Windows run `.\build_tests.ps1`.

## Reporting issues

Roger is a downstream fork feature — please report Roger issues on this
fork's issue tracker, **not** the ScummVM bug tracker. Include the game,
the display mode, and a screenshot (Side-by-Side mode captures the enhanced
and original views in one image).

## License

Roger is part of ScummVM and is licensed under the
**GNU General Public License, version 3 or later** — see
[COPYING](../../../COPYING) at the repository root. Every source file in
this directory carries the standard ScummVM GPL header.

No game data or proprietary assets are included; Roger generates its art
from the game files you already own.
