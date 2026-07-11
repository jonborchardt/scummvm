# Roger overlay compositor (`overlay/`)

The display layer: it composites the generated plate (from `../gen/`), the live
cast + captured sprites, and the SCI UI (dialogs, banner, buttons, edit fields,
icons) into the OSystem overlay surface. Everything here operates on surfaces and
the retained draw-journal — it does not itself generate art.

| File | Role |
|------|------|
| `roger_compositor.{h,cpp}` | The compositor: plate + priority-masked sprites + generic native regions + UI display-list → overlay; letterbox, dirty-rect present, `resetForRoomChange()` |
| `roger_journal.{h,cpp}` | Append-only `RogerJournal` — owns UI draw ordering + lifetime (supersede/prune/rollback) |
| `roger_ui_layer.h` | Resolution-independent `UiElement` struct |
| `roger_text.{h,cpp}` | TTF text fit/draw; hybrid ASCII-TTF + game-font-glyph `drawPx` layout |
| `roger_menu_model.{h,cpp}` | SCI-free menu-bar/dropdown state model (bar titles, dropdown rows, highlight), fed by the observer's menu events |
| `roger_effects.{h,cpp}` | Transition family mapping + blend functions (fade/dissolve/wipe/scroll, rolls → split/diagonal curtains); shake is composited in `roger_compositor` |
| `roger_cursor.{h,cpp}` | Composited overlay cursor |
| `roger_palette_remap.{h,cpp}` | Live-palette blend table (snapEga → liveEga) applied at composite time |
| `view_cache.{h,cpp}` | Serves upscaled hires VIEW cels (generates via `RogerAssetGen::generateViewCel`, caches owned) |
| `roger_coords.h` | Game-space ↔ overlay-space coordinate helpers |
| `roger_tokens.h` | Namespace token constants for journal element lifetime |

These units are deliberately **SCI-engine-type-free** so they unit-test in the
minimal CxxTest runner (`build_tests.ps1`). The invariants that govern this layer
(draw+lifetime mirroring, dirty-rect discipline, per-cycle O(1) cost) live in
`CLAUDE.md` → "SCI0 rendering & UI invariants" and "Performance discipline" — read
them before touching a hook or the present path.
