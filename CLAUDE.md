# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

ScummVM uses a configure + GNU Make build system on Linux/macOS. On Windows, use `devtools/create_project` to generate IDE project files.

### Windows (quick start)

A PowerShell script handles everything — installs dependencies via vcpkg, generates the VS solution, builds, and launches SQ3:

```powershell
.\build_and_run.ps1
```

- First run: ~15–30 min (vcpkg compiles SDL2, libpng, zlib etc. from source; cached after)
- Subsequent runs: ~1–3 min incremental build + launch
- Requires: Visual Studio 2019/2022 with "Desktop development with C++" workload

Game data: `J:\SteamLibrary\steamapps\common\Space Quest Collection\sq3`
Roger art: `J:\SteamLibrary\steamapps\common\Space Quest Collection\sq3-roger`

### Configure and build (Linux/macOS)
```sh
./configure [--enable-engine=<name>] [--disable-engine=<name>]
make -j$(nproc)
```

### Generate IDE project files (Windows/macOS IDE)
```sh
# From the build directory:
/path/to/scummvm/devtools/create_project /path/to/scummvm --msvc    # Visual Studio
/path/to/scummvm/devtools/create_project /path/to/scummvm --xcode   # Xcode
```

### Run unit tests
```sh
make test
```

Tests use the CxxTest framework located in `test/cxxtest/`. Test source files are in `test/`.

## Roger Project

This fork adds the **Roger** art replacement system for SCI0 games (SQ3, QFG1 EGA). It renders high-resolution backgrounds and VIEW cels **generated in-engine** from the SCI resources (the omyac upscaler pipeline), presented through ScummVM's OSystem overlay, while leaving all game logic intact. In-engine generation is the default and only art path — no pre-generated PNGs are consumed.

- Design spec (single living doc): `docs/superpowers/specs/2026-06-19-roger-art-replacement-design.md` — done/partial/future are stratified there; the superseded per-phase plans were removed (git history is the record of what shipped)
- User-facing docs: `docs/roger.md`
- All Roger code lives in `engines/sci/roger/`

> **Native rendering only.** Roger targets the native (desktop) ScummVM build. An
> earlier web/Emscripten/PixiJS prototype was abandoned; all of that code, build
> scripts, and the `roger-canvas` HTML overlay have been removed. The hires visual
> is displayed through ScummVM's **OSystem overlay** (a higher-resolution layer
> composited above the 320×200 game surface) — not a browser canvas.

### Stage 1: Background replacement

Hook at top of `GfxPaint16::drawPicture()` checks `g_sciRogerProvider`. When non-null and `hasBackground()` returns true (i.e. a generating `roger_gen_mode` is active), it lets SCI's **native picture render run** — which fills SCI's own 320×200 priority + control buffers, so walkability and native occlusion stay correct — then calls `pushHiresBackground()`. That generates (or loads from the content cache) the hires plate for the pic and presents it to the OSystem overlay. The overlay's per-pixel sprite occlusion is derived **in-engine** by rendering SCI's **priority screen** through the *same* omyac pipeline as the visual (`RogerAssetGen::generatePriorityMap()`, the `omyacprio` cache) — priority codes are EGA colours, so the output is a colour EGA priority view, upscaled/edge-enhanced exactly like the plate; the occlusion bands are recovered from that render (nearest EGA colour → code), not from any prebuilt map.

**Status: implemented + verified.** Supports SCI0 EGA games (SQ3, QFG1 EGA) with omyac upscaling. VGA games are detected at startup and rejected with a warning (EGA-only). Generation activates with zero prebuilt files; `pushHiresBackground()` presents the generated plate to the overlay immediately on room load (no native→hires "pop"). Walkability/native occlusion ride SCI's native buffers; overlay sprite occlusion uses the priority screen upscaled through omyac.

**Key files:**

| File | Role |
|------|------|
| `engines/sci/roger/roger_art_provider.h` | Abstract interface + `g_sciRogerProvider` global |
| `engines/sci/roger/roger_asset_gen.h/cpp` | In-engine generation: `generatePlate()` (omyac plate for EGA), `generateViewCel()` (scale6x cel, EGA only), `generatePriorityMap()` (renders SCI's priority screen through omyac; overlay occlusion bands recovered from it), `priorityBands()` (legacy native occlusion bands), `generateTextSurface()` (renders one native-font glyph via `scaleNearest` → RGBA, for the hybrid text path), backed by a content-hash disk cache (`kTransformVersion`-keyed) |
| `engines/sci/roger/roger_pic_native.{h,cpp}` + `roger_pic_parser` / `roger_omyac` / `roger_scale` / `roger_ega_blend` | The omyac pipeline: parse pic → native pre-render (exposes `NativeRef::priority`) → enhance passes → RGBA plate; scale6x for VIEW cels |
| `engines/sci/roger/file_roger_art_provider.h/cpp` | Provider: `hasBackground()` (generating-mode gate), `pushHiresBackground()` (generates+presents the plate, routes occlusion through `generatePriorityMap()` — hires omyac-aligned), `precacheAll()`, scene/UI capture (incl. `buildGlyphs()` — pre-renders each non-ASCII byte from the game font for the hybrid text path), status-banner cache, cursor policy |
| `engines/sci/roger/roger_compositor.h/cpp` | Composites plate + sprites (priority-masked) and the UI display-list (dialogs/banner/buttons/edit/icons) into the overlay; opaque-black letterbox; black dialog borders |
| `engines/sci/roger/roger_text.h/cpp` | TTF text fit/draw; type scale driven by captured native SCI font metrics (per-element target cell height + single-line width cap — same on-screen footprint as the original), falling back to role heights when no metric was captured; `firstLineTop`/`vAlignTop`, and the hybrid `drawPx` layout: ASCII drawn with the TTF font, each non-ASCII byte blitted inline as the game's own font glyph (from the element's glyph map, scaled to ¾ line height) |
| `engines/sci/roger/roger_ui_layer.h` | Resolution-independent `UiElement` display-list |
| `engines/sci/roger/view_cache.h/cpp` | Serves upscaled hires VIEW cels for ego/props/inventory by generating them on first use via `RogerAssetGen::generateViewCel` and caching them (owned). No prebuilt spritesheets. |
| `engines/sci/roger/png_loader.h/cpp` | `loadGrayscale8()` / `loadSurfaceRGBA()` via `Image::PNGDecoder` |
| `engines/sci/graphics/{paint16,controls16,menu,event}.cpp` | Hook sites: picture replace, dialog/control capture, status/menu bar, F10 toggle |
| `engines/sci/sci.cpp` | Provider instantiated after `initGraphics()` (with `ConfMan.getPath("path")`), destroyed in destructor |

**Asset layout** (`sq3-roger` is a sibling of the `sq3` game directory). Nothing is *consumed* from disk anymore — the only on-disk artifacts are the content-hash generation cache:
```
sq3-roger/
  cache/
    <gameid>.omyac.v<ver>.<hash>.<passes>.png    ← generated hires plate (per pic, content-keyed)
    <gameid>.scale6x.v<ver>.<hash>.<passes>.png  ← generated hires VIEW cel (per view/loop/cel)
    <gameid>.omyacprio.v<ver>.<hash>.<passes>.png ← priority screen rendered through omyac, colour EGA priority view (per pic, content-keyed)
```
Plates and VIEW cels are generated in-engine from the SCI resources and written here on a cache miss (modes `cache`/`always`); `memory` generates without writing. The cache key embeds `kTransformVersion`, so a pipeline change invalidates stale files automatically.

**Config knobs** (all `ConfMan.hasKey(...)`-gated; see `docs/roger.md` for the full table): `roger_gen_mode` (default `cache`; `prebuilt` = native-only off-switch, `memory`, `always`), `roger_precache` (default `all`; `pics`|`views`|`off`), `roger_omyac_passes`, `roger_ui_font_scale` (default 100), `roger_ui_font`, `roger_ui_header_font`, `roger_hw_cursor`, `roger_cursor_size`, `roger_dirty_present` (default on; convert+push only the changed regions each frame — dirty-rectangle present — set false to force a full-region present), `roger_transitions` (default on; mirrors SCI fade/dissolve/wipe/scroll and shake in the overlay; Wipe/Scroll currently render as Dissolve), `roger_palette_live` (default on; re-applies the live SCI EGA palette to the hires plate each frame for cycling, fade-to-black, and flash effects), `roger_autoshot`, `roger_debug`. The `roger_visual_variant`/`roger_priority_variant` knobs are retired (they selected prebuilt files). F10 A/B toggles the overlay on/off live; Ctrl+Shift+[ ] / ; ' tune enhance passes live. Ctrl+Shift+F cycles the dialog/body font through a fonts.dat shortlist.

**Integration test:** Room 2 (pic resource 2). Walkability/native occlusion ride SCI's native render; overlay sprite occlusion uses the omyac-rendered priority screen (`omyacprio`, colour), so occlusion edges get the same upscaling as the plate.

**Tests:** `test/sci/roger/` (CxxTest) — require a `make`-based build to run (SCI as a static plugin).

### Stage 2: Native hires overlay compositor (implemented + verified)

The compositor draws the ego/props into the OSystem overlay at hires (upscaled native cels via the ViewCache, or rendered native cels as fallback) with SCI priority-band masking against the replacement art, and composites the SCI UI that would otherwise be hidden under the overlay: dialog windows (black border), the score/title banner (cached + re-applied on room load and F10 enable), buttons, top-aligned text-edit fields with a live caret, and inventory icons / look-at close-ups. The letterbox is filled opaque black so the native render (and its hardware cursor) cannot leak at the edges, and the cursor itself is the native hardware cursor (SCI sets arrow/wait/hand; smooth, correct over the overlay). All game logic stays at 320×200; only the display layer is hires.

The hires priority map for sub-pixel occlusion alignment is now generated in-engine (omyac-aligned `omyacprio` cache), so overlay occlusion tracks the displayed plate. Remaining art-side work: authoring better hires backgrounds and new hires VIEW art for room sprites.

The overlay present is **dirty-rectangle by default** (`roger_dirty_present`): each frame converts+pushes only the regions that actually changed (sprites, cursor, UI) plus the union of the previous frame's, instead of the whole game region — at 2862×1986 this cut present from ~26 ms to ~2 ms. Sprite rects are tracked at *renderScene* granularity and UI/cursor rects at *present* granularity (see `roger_compositor.cpp` `dirtyUnion`/`rollPresentDirty`), so a UI-only present (`presentWithUi`: cursor move / dialog, no `renderScene`) cannot discard sprite-erase history. Room change / geometry / F10 and a periodic heal frame still do a full present; any uncertainty falls back to a full present (never a skipped/garbage frame).

### Stage 3: Plugin migration (future)

When Roger becomes its own plugin, these existing files must be revisited — all other changes are in `engines/sci/roger/` which will move wholesale:

| File | What changes |
|------|-------------|
| `engines/sci/graphics/paint16.cpp` | Decouple the hard-coded `g_sciRogerProvider` global — SCI engine needs to expose a registration API (e.g. `setArtProvider()`) that the plugin calls at load time |
| `engines/sci/sci.cpp` | Remove include, instantiation (`new FileRogerArtProvider(...)`), and destruction — plugin self-registers via the new API |
| `engines/sci/module.mk` | Remove the `# Roger art replacement` block — `roger/*.o` files move to the plugin's own `module.mk` |
| `test/module.mk` | Change test linking from `engines/sci/libsci.a` to a roger-specific static library |

## Code Style

- **C++11**, tabs for indentation (width 4), no column limit
- Pointer/reference aligned to the right: `int *ptr`, `void foo(int &bar)`
- Braces attached (K&R style): `if (x) {`
- `.clang-format` is present and enforces these rules
- No exceptions (`-fno-exceptions`), no RTTI
- All code must be GPLv3+ compatible

## Repository Architecture

### Core modules (shared across all engines)

| Directory | Purpose |
|-----------|---------|
| `common/` | Shared utilities: strings, streams, containers, file system, config manager, archive formats, event manager |
| `graphics/` | 2D rendering: surfaces, pixel formats, font management, scalers, Mac GUI widgets |
| `audio/` | Audio mixing, MIDI drivers, codec decoders |
| `video/` | Video codec decoders (Bink, Smacker, QuickTime, etc.) |
| `image/` | Image format decoders (PNG, JPEG, BMP, etc.) |
| `math/` | Math utilities (vectors, matrices, frustum) |
| `gui/` | ScummVM launcher GUI, dialog system, theme engine |
| `base/` | Program entry point (`main.cpp`), plugin manager, command-line parsing |

### Platform abstraction

`backends/` contains platform-specific implementations. All platforms implement the `OSystem` interface defined in `common/system.h`. The SDL backend (`backends/platform/sdl/`) is the primary desktop backend. Other backends include Android, iOS, libretro, and various consoles.

### Engine plugin system

Each game engine lives in `engines/<name>/` and integrates via:

- **`configure.engine`** — declares the engine to the build system (`add_engine` macro)
- **`module.mk`** — lists all `.o` files to compile
- **`MetaEngineDetection`** — handles game detection (can be compiled without the full engine for the detection plugin)
- **`MetaEngine`** — creates `Engine` instances, manages save states, provides GUI options
- **`Engine`** subclass — implements `run()` as the main game loop

The plugin system (`base/plugins.h`) supports both static linking and dynamic ELF plugins.

### Game detection

Most engines use the `AdvancedDetector` framework (`engines/advancedDetector.h`). Detection tables in `engines/<name>/detection_tables.h` list `ADGameDescription` entries with filename + MD5 pairs. The detector matches game files against these tables to identify the specific game version.

### Director engine (engines/director/)

The Director engine implements Macromedia/Macromedia Director games. Key components:

- **`DirectorEngine`** — top-level engine, manages windows and global state
- **`Movie`** — represents a Director movie file (`.DIR`/`.DXR`/`.MMM`)
- **`Cast`** — resource library holding all `CastMember` objects (bitmaps, sounds, scripts, text, shapes, etc.)
- **`Score`** — the timeline/sequencer; contains `Frame` objects that define which channels are active each frame
- **`Channel`** — one sprite slot in a frame
- **`Window`** — a stage or MIAW (Movie In A Window)
- **`Lingo`** — the scripting engine for the Lingo language (in `lingo/`)

The Lingo subsystem uses bison/flex (`lingo-gr.y`, `lingo-lex.l`) to parse scripts and compiles them to bytecode. XObject/XLib support is in `lingo/xlibs/` and `lingo/xtras/`.

## Developer Tools

- **`devtools/make_class.py`** — scaffolds a new C++ class in an engine (creates `.cpp`/`.h`, updates `module.mk`):
  ```sh
  python3 devtools/make_class.py scumm . LeChuck        # engines/scumm/le_chuck.{cpp,h}
  python3 devtools/make_class.py director lingo MyXObj  # engines/director/lingo/my_x_obj.{cpp,h}
  ```
- **`devtools/create_engine/`** — scaffolds a new engine
- **`devtools/dumper-companion.py`** — dumps HFS/HFS+ volumes and Mac game files
- Various `devtools/create_<engine>/` tools generate `.dat` data files for specific engines

## Adding a New Engine

1. Create `engines/<name>/` with `configure.engine`, `module.mk`, `detection.cpp`, `metaengine.cpp`, and the main engine class
2. Run `./configure` to pick up the new engine
3. Add detection entries in `detection_tables.h` using `ADGameDescription` structs and `AD_ENTRY*` macros
