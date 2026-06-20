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

This fork adds the **Roger** art replacement system for SCI0 games (SQ3, QFG1 EGA). It substitutes pre-generated high-resolution PNG backgrounds, priority maps, and control maps for SCI's native vector/cel rendering, while leaving all game logic intact.

- Design spec: `docs/superpowers/specs/2026-06-19-roger-art-replacement-design.md`
- Implementation plan (native overlay): `docs/superpowers/plans/2026-06-20-roger-native-overlay.md`
- All Roger code lives in `engines/sci/roger/`

> **Native rendering only.** Roger targets the native (desktop) ScummVM build. An
> earlier web/Emscripten/PixiJS prototype was abandoned; all of that code, build
> scripts, and the `roger-canvas` HTML overlay have been removed. The hires visual
> is displayed through ScummVM's **OSystem overlay** (a higher-resolution layer
> composited above the 320×200 game surface) — not a browser canvas.

### Stage 1: Background replacement

Hook at top of `GfxPaint16::drawPicture()` checks `g_sciRogerProvider`. When non-null and `hasBackground()` returns true, it fills the **320×200 priority + control buffers** from PNG (so SCI pathfinding/occlusion honor the replacement) and returns early (skipping SCI vector rendering). The hires visual is then shown via the OSystem overlay (`pushHiresBackground()`).

**Status:** the hook + priority/control buffer replacement are implemented and verified natively (ego walkability and sprite occlusion respond correctly to swapped maps). `pushHiresBackground()` is currently a **stub** — the native OSystem-overlay display is the next implementation step (see design spec).

**Key files:**

| File | Role |
|------|------|
| `engines/sci/roger/roger_art_provider.h` | Abstract interface + `g_sciRogerProvider` global |
| `engines/sci/roger/file_roger_art_provider.h/cpp` | File-based provider: path construction, visual-variant selection, `hasBackground()`, `loadBuffers()`, `pushHiresBackground()` (OSystem overlay — TODO) |
| `engines/sci/roger/png_loader.h/cpp` | `Sci::Roger::loadGrayscale8()` via `Image::PNGDecoder` |
| `engines/sci/graphics/paint16.cpp` | hook at top of `drawPicture()` |
| `engines/sci/sci.cpp` | Provider instantiated after `initGraphics()` (with `ConfMan.getPath("path")`), destroyed in destructor |

**Asset layout** (`sq3-roger` is a sibling of the `sq3` game directory):
```
sq3-roger/
  pics/
    <id>/
      source/
        pic.<id>.png                 ← low-res original visual
        pic.<variant>.<id>.png       ← hires visual, e.g. pic.omyac-upscaler.<id>.png
        pic.<id>_p.png               ← priority map (must be exactly 320×200, grayscale)
        pic.<id>_c.png               ← control map (must be exactly 320×200, grayscale)
```
The hires visual variant is selected by config key `roger_visual_variant` (default `omyac-upscaler`; empty string uses the plain `pic.<id>.png`).

**Integration test:** Room 2 (pic resource 2), art at `sq3-roger/pics/2/source/`. To verify the buffers are honored, swap in deliberately-wrong uniform priority/control maps and observe ego occlusion/walkability change.

**Tests:** `test/sci/roger/` (CxxTest) — require a `make`-based build to run (SCI as a static plugin).

### Stage 2: Native hires overlay compositor (main remaining work)

Composite the ego/props (and the SCI UI elements that would otherwise be hidden under the overlay) into the OSystem overlay at hires, with SCI priority-band masking against the replacement art. Keeps all game logic at 320×200 in ScummVM; only the display layer is hires.

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
