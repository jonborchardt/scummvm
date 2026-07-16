# Web Demo — Emscripten Build and Bundle

**Downstream-only.** This document, the WSL/emscripten build flow it describes,
and the dev bundle it produces are fork-maintenance material — none of it is
part of any upstream PR. Upstream ScummVM's own emscripten port is unaffected;
Roger's web demo is a downstream packaging exercise on top of it.

This is a factual build/run record, re-derivable from a clean WSL install. It
covers everything through demo assembly, including the Phase 4 Betrayed
Alliance bundle (see "Phase 4 bundle" below); it does not cover deployment
(hosting, CI publishing, permanent asset placement) — that is Phase 5
territory and belongs in a later doc.

## Overview

Roger's native target is Windows/MSVC (see the top-level `CLAUDE.md`). The web
demo is a **second, WSL-hosted build** of the same source tree targeting
emscripten/wasm, produced from a dedicated Linux-side clone so the Windows
checkout and its build state are never touched. The flow has six stages:

1. WSL clone of the Windows repo, on its own branch (`jon-wasm`).
2. A native Linux build + `make test`, as a portability check before touching
   emscripten (catches MSVC-only assumptions early, cheaply).
3. An emscripten build of the same source.
4. A `dist` bundle: engine + game data + a warmed Roger generation cache,
   served locally over plain HTTP for browser verification.
5. A **packaged game-data** preload step (Phase 2.5) that ships the game
   directory and cache as a single Emscripten preload package instead of
   many per-file HTTP fetches — see "Packaged game data" below. This is
   now the primary path; the plain-HTTP layout from stage 4 remains as the
   fallback when no package is present.
6. The **Phase 4 bundle-content swap**: Betrayed Alliance replaces SQ3 as
   the staged game/cache content in the assembled bundle, with the shipped
   ini, no-fragment boot default, and branded landing page — see "Phase 4
   bundle (Betrayed Alliance)" below.

## WSL clone and push-back flow

Work happens in a **separate WSL-side clone**, not directly against the
`E:\` drvfs mount — building or running git operations straight against
`/mnt/e/...` is slow (drvfs) and trips `fatal: detected dubious ownership`.
The one-time WSL environment setup:

```sh
wsl.exe -u root bash -lc "apt-get update && apt-get install -y \
    build-essential git python3 pkg-config wget \
    libsdl2-dev libsdl2-net-dev zlib1g-dev libpng-dev libfreetype6-dev libjpeg-dev \
    python-is-python3 zip"
```

`python-is-python3` and `zip` are not part of a default Ubuntu image and are
both required later (see Gotchas). Then clone WSL-side, adding the
`safe.directory` exceptions the drvfs-mounted origin needs:

```sh
git config --global --add safe.directory /mnt/e/github2/scummvm
git config --global --add safe.directory /mnt/e/github2/scummvm/.git
git clone /mnt/e/github2/scummvm ~/scummvm-wasm
cd ~/scummvm-wasm
git checkout -b jon-wasm
git config user.name  "<your name>"
git config user.email "<your email>"
```

The WSL clone's `origin` is the Windows checkout itself (`/mnt/e/github2/scummvm`),
so push-back is a normal push — no separate remote or PAT needed:

```sh
git push origin jon-wasm
```

This makes `jon-wasm` visible as a local branch in the Windows repo
(`git branch -a --list "*wasm*"`) without ever checking it out there.  All
work in this doc happens inside `~/scummvm-wasm`, branch `jon-wasm`; the
Windows working tree stays on its own branch and is never dirtied.

## Native Linux build (portability check)

Run before touching emscripten — it isolates "compiles under gcc" issues
from "compiles under emscripten" issues:

```sh
./configure --disable-all-engines --enable-engine=sci
make -j$(nproc)
make test
```

`make test` requires the roger suite's test objects to link against
`test/module.mk`'s `TEST_LIBS` (a small explicit SCI-free object list plus a
stub translation unit standing in for the engine symbols the Windows path
dead-strips via `/Gy`+`/OPT:REF` — GNU ld has no equivalent forgiveness for
whole-archive extraction). This wiring already exists in-tree
(`test/sci/roger/roger_test_stubs.cpp`); if it is ever missing, the failure
mode is hundreds of undefined-reference link errors naming `Engine::*`,
`MetaEngine`, and similar full-application symbols, not a Roger-specific
error.

## Emscripten build

**Command-contract amendment — read before running any `build.sh` command.**
The bare form from upstream's own docs,

```sh
./dists/emscripten/build.sh configure --disable-all-engines --enable-engine=sci
```

**succeeds (exit 0) but silently drops PNG and FreeType support** —
`config.h` comes out with `#undef USE_PNG` / `#undef USE_FREETYPE2`. On the
emscripten target, `configure` forces auto-detected optional libraries OFF
unless explicitly `--enable-*`d (upstream's own emscripten CI passes the full
`--enable-gif --enable-jpeg --enable-ogg --enable-png --enable-vorbis
--enable-zlib --enable-freetype2` set for this reason). Roger needs PNG for
the generation cache and FreeType for the TTF text path; without them the
web build still boots but Roger's enhanced rendering is silently degraded.
**Every `build.sh` invocation for this project must carry:**

```sh
--enable-png --enable-freetype2 --enable-zlib
```

The full three-stage build:

```sh
./dists/emscripten/build.sh configure --disable-all-engines --enable-engine=sci \
    --enable-png --enable-freetype2 --enable-zlib

./dists/emscripten/build.sh make --disable-all-engines --enable-engine=sci \
    --enable-png --enable-freetype2 --enable-zlib
```

The first `configure` run auto-installs emsdk (~300 MB, node + clang
wasm-binaries) under `dists/emscripten/emsdk-<version>/`; keep the WSL
session alive for the duration (a backgrounded `&`/`disown` process died
mid-download in practice — run it in the foreground, or under a session that
outlives the download, not detached during install).

`make` produces `scummvm.html` / `scummvm.js` / `scummvm.wasm` at the repo
root. As of the 2026-07-12 measurement: `scummvm.wasm` is 14,007,161 bytes
(~14 MB), `scummvm.js` ~234 KB, `scummvm.html` ~4 KB.

Note that running the emscripten `configure` overwrites the native build's
`config.h`/`config.mk`/engine tables in the same tree — re-run native
`./configure` first if you need to go back to a native build afterward.

## Bundle assembly

**This is the fallback layout** — the plain-HTTP, one-fetch-per-file path the
shell degrades to when no packaged game-data blob is present (see "Packaged
game data" below for the primary path). It is still required as the base
`dist` step either way: the packaged flow layers on top of it, it does not
replace it.

```sh
./dists/emscripten/build.sh dist --disable-all-engines --enable-engine=sci \
    --enable-png --enable-freetype2 --enable-zlib
```

This produces `build-emscripten/` containing the engine artifacts, ScummVM's
own data archives (`fonts.dat`, `translations.dat`, `gui-icons.dat`, theme
zips, etc.), and an empty `data/games/` directory. The bundle layout Roger
adds on top:

```
build-emscripten/
  scummvm.html  scummvm.js  scummvm.wasm
  scummvm.ini                       <- shipped config (see below)
  data/
    index.json                      <- top-level HTTP index
    fonts.dat  translations.dat  gui-icons.dat  ...  (stock ScummVM data)
    games/
      sq3/                          <- real game data, copied verbatim
        resource.map  resource.001  RESOURCE.002  RESOURCE.003  ...
        index.json
      sq3-roger/
        index.json
        cache/                     <- warmed Roger generation cache
          index.json               <- served manifest the provider fetches
          sq3.omyac.v<ver>.<hash>.<passes>.png
          sq3.omyacprio.v<ver>.<hash>.<passes>.png
          sq3.scale6x.v<ver>.<hash>.<passes>.png
          sq3.done.v<ver>.<passStamp>.marker
```

Game data is copied in from a real local install (never committed) — only
needed for the HTTP-FS fallback layout; the packaged flow ships game data
inside `scummvm-game.data` instead:

```sh
cp -r "/path/to/Space Quest Collection/sq3" build-emscripten/data/games/sq3
```

The `<gameid>-roger/cache/` sibling mirrors the desktop on-disk layout
(`docs/roger/DATA_LAYOUT.md`) exactly — same filename cache-key scheme,
same three transforms (`omyac`, `omyacprio`, `scale6x`). To enable enhanced
mode in the web demo, copy a cache already warmed by a desktop precache run
into that sibling path before regenerating the HTTP index (below). At the
2026-07-12 measurement, the SQ3 cache was 8,369 files (253 omyac + 253
omyacprio + 7,862 scale6x + 1 marker) and reported ~53 MB after copying
drvfs→ext4 (file *count* is the check that matters; the byte-size delta from
the source tree's reported 31.2 MB is a filesystem block-size artifact of
the copy, not a content discrepancy).

## Packaged game data (Phase 2.5)

The plain-HTTP layout above serves every game and cache file as its own
synchronous XHR. At SQ3's cache size that meant thousands of individual
fetches, each one blocking the SCI game cycle for the duration — the root
cause of the multi-second scene-change freezes in the original perf
snapshot (see "Performance: packaged vs HTTP-FS" below). The fix packages
the game directory and current-key cache into a single Emscripten preload
blob with `file_packager.py`, mounted into MEMFS instead of the lazy HTTP
filesystem, so reads become in-memory instead of one round-trip each.

`dists/emscripten/build-package_game.sh <game-dir> <cache-dir> <gameid>
<cache-ver> <passes-token>` builds the package. Exact working example:

```sh
dists/emscripten/build-package_game.sh \
  "/mnt/j/SteamLibrary/steamapps/common/Space Quest Collection/sq3" \
  "/mnt/j/SteamLibrary/steamapps/common/Space Quest Collection/sq3-roger/cache" \
  sq3 v6 p0p2p2p2p2p2p1p0p0p0
```

`<cache-ver>` and `<passes-token>` are an **orphan-selection filter, not
free-form labels**: the script only stages cache files matching
`<gameid>.*.<cache-ver>.*.<passes-token>.png`, so stale versions or old
pass-sets sitting alongside the current ones in the master cache directory
are excluded by construction — no separate cleanup step is needed. Run
`dists/emscripten/build.sh dist` first; the script requires
`build-emscripten/` to already exist. For the SQ3 example above this staged
4,001 cache files (116 omyac + 116 omyacprio + 3,769 scale6x) plus 28 game
files, producing `build-emscripten/scummvm-game.{data,js}` at 18.3 MB total.

**Never pass `--use-preload-cache`** to `file_packager.py` — that copies the
package into IndexedDB, and browser storage here must hold saves/config
only, not game data.

**Mount point is `/gamedata`, not `/data` — this is required, not a style
choice.** ScummVM's Emscripten filesystem factory
(`backends/fs/emscripten/emscripten-fs-factory.cpp`) routes *all* paths
under `/data/*` to its per-file synchronous-XHR HTTP filesystem
unconditionally, with no opt-out. A preload package staged under `/data`
would therefore be invisible to the engine even though the browser has it
in memory — the FS factory would still issue one HTTP fetch per file. Only
`/gamedata` falls through to the real Emscripten POSIX filesystem (MEMFS),
so the shipped ini must point at `path=/gamedata/games/<gameid>` (see
"Shipped `scummvm.ini`" below), and `data/games/` in the plain-HTTP layout
is no longer used at all in the packaged flow — the HTTP index only needs
to cover engine data (`fonts.dat`, `translations.dat`, theme zips, etc.),
not game or cache files.

The shell loads the package via `custom_shell.html`:

```html
<script src="scummvm-game.js" onerror="console.warn('scummvm-game.js not present; using HTTP filesystem only')"></script>
```

This `<script>` tag sits between the `var Module = {...}` definition and the
async engine script, so the packaged data mounts into MEMFS before `main()`
runs. It is deliberately **parser-blocking** (no `async`/`defer`) for that
ordering guarantee. If `scummvm-game.js` is absent (no package was built
for this bundle), `onerror` logs a warning and the engine falls straight
back to the plain-HTTP `data/games/` layout above — the degrade path is
automatic, no ini change required.

## Phase 4 bundle (Betrayed Alliance)

Phase 4 replaces the SQ3 dev bundle above with the public demo game,
**Betrayed Alliance Book 1 v1.3.3.1** (game id `sci-fanmade`; pinned/licensed
per the spec's §4). The mechanics below are additive to everything above —
same packaged-flow (Phase 2.5) machinery, same `/gamedata` mount, same
`build.sh dist` base step — only the staged game/cache content and shipped
ini target a different game.

**Minimal staging.** Only three files ever leave the release download: a
staging directory holding exactly `resource.map` + `resource.001` +
`LICENSE.TXT`, copied from the local release
(`J:\BetrayedAllianceBook1-v1.3.3.1\...`) — never the bundled DOSBox/SDL/
drivers/interpreter EXEs/`SRC/`. From `~/scummvm-wasm` with that staging dir
already populated (e.g. `~/ba-game-stage`):

```sh
dists/emscripten/build-package_game.sh ~/ba-game-stage \
  '/mnt/j/BetrayedAllianceBook1-v1.3.3.1/sci-fanmade-roger/cache' \
  sci-fanmade v6 p0p2p2p2p2p2p1p0p0p0
```

Expected output: `staged: 3 game files, 4549 cache files`, producing
`build-emscripten/scummvm-game.data` at **25,720,155 bytes (~24.5 MiB)**.
**Note:** `file_packager.py` writes its progress as carriage-return-only
updates, which can visually garble the staged-count echo line in a scrollback
buffer — if the count looks suspicious, verify with `find <staging-dir> -type
f | wc -l` / `find <cache-dir> -type f -name '*.png' | wc -l` rather than
re-reading the terminal text.

`LICENSE.TXT` is also copied to `build-emscripten/LICENSE-BetrayedAlliance.txt`
(served at the bundle root; the landing page links it — see below).

**Shipped ini is a tracked file, copied in, not hand-edited.**
`dists/emscripten/roger-demo.ini` is the single source of truth for
`build-emscripten/scummvm.ini`; `build.sh dist` does not copy it
automatically, so re-copy it after every dist rebuild:

```sh
cp dists/emscripten/roger-demo.ini build-emscripten/scummvm.ini
```

(Verify with `grep betrayed build-emscripten/scummvm.ini` — a dist rebuild
that clobbers the ini without a re-copy silently reverts the bundle to
whatever ini shape `build.sh dist` last generated.) Remember the emscripten
runtime persists `scummvm.ini` into IndexedDB on first boot (see Dev
gotchas above), so any change to the shipped ini needs an IndexedDB clear
from the directory-listing page to take effect on a subsequent load.

**No-fragment default.** `custom_shell-pre.js` (commit `111cb8a282a`) pushes
`betrayed` onto `Module["arguments"]` when `window.location.hash` is empty —
`roger_no_launcher=true` alone still leaves a bare argv falling through to
the stock ScummVM launcher, so the pre-js default is what makes a plain
`scummvm.html` cold load boot straight into the game. A URL fragment (e.g.
`#sq3` during dev) still wins unchanged when present.

**Landing page.** `custom_shell.html` (commit `6b49f93d075`) replaces the
stock emscripten shell with one screen: the game canvas over a slim info bar
carrying a one-line description of Roger, the F10 display-mode hint
(Enhanced → Original → Side-by-Side), parser/click control notes, and a
Ryan Slattery / Slattstudio credit line linking `slattstudio.com`,
`github.com/Slattstudio/BetrayedAllianceBook1`, and the bundle-root
`LICENSE-BetrayedAlliance.txt`, plus a browser-storage-saves caveat. The
debug output textarea from the stock shell is gone; engine output goes to
the console only.

**Side-by-side treatment (decided by trying both, per spec §5 Phase 4).**
The user tried both variants at the wizard room and chose **Variant B: boot
straight into full-screen Enhanced, with the info bar's F10 hint as the
discovery path** — no `roger_display_mode` key ships in `roger-demo.ini`.
The mechanism for Variant A (boot straight into Side-by-Side) remains
available and was verified working this phase: add
`roger_display_mode=sbs` under `[betrayed]` in `roger-demo.ini` and rebuild.

**Measured sizes (2026-07-12, Chrome via Playwright, full verification
sweep).** All six sweep items passed: cold load with no ScummVM chrome,
parser round-trip, walk/scene sanity, F10 one-step-per-press across all
three display modes, save/reload via IDBFS, and a clean console. The
item-by-item evidence lives in the local-only sweep report
(`.superpowers/sdd/task-6-report.md`, screenshots `screenshots/phase4/` —
neither is tracked; the "Gate snapshot" section further below is the
earlier Phase 1/2 SQ3 data, not this sweep).

| Artifact | Size |
|---|---|
| `build-emscripten` total (`du -sh`) | 55 MB |
| `scummvm-game.data` | 25,720,155 bytes (~24.5 MiB) |
| `scummvm.wasm` | 14,007,305 bytes (~13.4 MiB; a later relink of the same source than the 14,007,161-byte figure quoted elsewhere in this doc — shell-only relinks re-emit the wasm) |

Cold load to the title screen: ~3-6 s (localhost). All within the spec §3
expected range (~45-55 MB total / ~25 MB `.data` / ~14 MB wasm) and far
under the ~500 MB budget and GitHub Pages per-file/site caps.

**Known ship-visible residuals** (already-filed follow-ups, not Phase 4
defects):

- The Enhanced-mode status bar renders BA's blackletter title glyphs as
  literal ASCII punctuation (`$etrayed #lliance: $ook I`) — BA stores its
  fancy glyphs at ASCII codepoints, and Roger's TTF status-bar path draws the
  raw bytes. Original mode (native bitmap font) renders the title correctly;
  this is TTF-hybrid-path-specific.
- The known SDL3 synthetic-typing filter edge (the machine-speed phantom
  duplicate-character residual from Phase 2.6 — real human typing is clean)
  has a new data point from this phase's verification sweep: it reproduces
  at 500-700 ms synthetic keystroke gaps in the in-game parser box, and in
  the native ScummVM GMM save-name widget (a different code path) even at
  1000 ms gaps.

## Shipped `scummvm.ini`

Two variants, differing only in `roger_gen_mode`. Both boot straight into the
game with no launcher and no confirm-exit modal (the emscripten runtime has
no way to answer either):

**Native-only (Roger inert, Phase 1):**

```ini
[scummvm]
roger_no_launcher=true
roger_gen_mode=prebuilt
confirm_exit=false
gui_return_to_launcher_at_exit=false

[sq3]
description=Space Quest III (dev bundle - never deployed)
gameid=sq3
engineid=sci
path=/gamedata/games/sq3
```

**Enhanced mode (Phase 2, cache shipped in the bundle):**

Identical except `roger_gen_mode=cache`. This is the only line that
changes between the two variants.

**`path=/gamedata/games/<gameid>` is the packaged-flow contract** (Phase
2.5, above) — it must match whatever gameid the package was built with.
When falling back to the plain-HTTP layout with no package present, use
`path=/data/games/<gameid>` instead; nothing else in the ini changes
between the two.

`roger_no_launcher=true` is required — the picker cannot be driven
headlessly and there is no keyboard/mouse operator at first boot in an
automated context. `confirm_exit=false` and
`gui_return_to_launcher_at_exit=false` are required for the same reason
`build_and_run.ps1 -Script` runs need them on desktop: nothing can answer a
confirm modal, and return-to-launcher means the process/tab never settles.

## HTTP index regeneration

The emscripten runtime's virtual filesystem consults a per-directory
`index.json` manifest to know what exists (and its byte size) before issuing
any fetch — a file absent from the index is never requested at all, even as
a 404. Regenerate it after **any** change to bundle contents (ini edits or
data changes under `data/`):

```sh
python3 dists/emscripten/build-make_http_index.py build-emscripten/data
```

**With the packaged flow (above), this only needs to cover engine data**
(`fonts.dat`, `translations.dat`, `gui-icons.dat`, theme zips) — game and
cache files live in `scummvm-game.data`/MEMFS, not under `data/games/`, so
there is nothing game-related left for the index to describe. The
`data/games/<gameid>-roger/cache/index.json` manifest below only applies to
the plain-HTTP fallback layout, where it is still the file the Roger
provider reads to know what's cached.

## Local serve and launch

```sh
cd build-emscripten
python3 -m http.server 8080
```

To keep the server alive independent of the invoking shell/session:

```sh
nohup python3 -m http.server 8080 > /tmp/http-serve.log 2>&1 & disown
```

Launch a specific game via the URL fragment — the fragment is read by
`scummvm.js` to select which configured game target to auto-launch:

```
http://localhost:8080/scummvm.html#sq3
```

## Dev gotchas

- **IndexedDB holds the persisted `scummvm.ini` (and saves).** The emscripten
  runtime copies `scummvm.ini` into IndexedDB-backed IDBFS on first launch
  and reads the *persisted* copy thereafter — editing the on-disk ini in the
  bundle has no effect until IndexedDB is cleared. Clear it from the
  **directory-listing page** (`http://localhost:8080/`), not the live game
  page: calling `indexedDB.deleteDatabase()` while `scummvm.html` is loaded
  and holding an open IDBFS connection **hangs indefinitely** — the call
  blocks until every connection to that database closes, and the running
  module never releases it.
- **Same-fragment navigation does not reload.** Navigating from
  `scummvm.html#sq3` back to `scummvm.html#sq3` is treated by the browser as
  a same-document navigation; the already-running module stays alive and
  none of the reload-dependent state (ini, cache) actually resets. Bounce
  through a different URL first — the plain directory-listing page works,
  since it doesn't run the emscripten module — then navigate back.
- **The browser's own HTTP cache masks server-side file changes** across
  "different" navigations sharing a profile: a file already fetched once can
  keep returning 200 from cache even after it's deleted or replaced on the
  server, silently defeating a miss/regression test. Clear it explicitly
  (e.g. a CDP `Network.clearBrowserCache` call) whenever a test's premise
  depends on the network actually being hit.
- **Use a fresh/incognito profile for a clean boot** — it's the simplest way
  to guarantee empty IndexedDB without hand-clearing it.
- **drvfs vs ext4:** build and git operations against a WSL-native ext4 clone
  (`~/scummvm-wasm`) are the supported path; running directly against the
  `/mnt/e/...` drvfs mount is slower and (for git) triggers "dubious
  ownership" until `safe.directory` is configured.
- **Pre-JS edits require a real relink.** `custom_shell-pre.js` (used to
  inject one-off diagnostics, e.g. cycle telemetry) is not tracked as a link
  dependency by the emscripten Makefile — a bare `build.sh make dist` after
  editing it silently re-copies the *stale* `scummvm.js`/`scummvm.wasm` into
  `build-emscripten/` without re-invoking `emcc`. Force a real link by
  deleting the root-level `scummvm.html`/`scummvm.js`/`scummvm.wasm` before
  re-running `build.sh`; confirm with a `LINK scummvm.html` line in the
  build log (and/or `grep` the injected marker string in the resulting
  `scummvm.js`).
- **WSL package gaps beyond the base dependency list:** `zip` (the `dist`
  target's Makefile shells out to it while packing `shaders.dat` and fails
  with `zip: No such file or directory` if it's missing) and
  `python-is-python3` (`test/cxxtest/bin/cxxtestgen` has a `#!/usr/bin/env
  python` shebang; a fresh Ubuntu image ships only `python3`). Both are
  included in the setup command above.
- **SQ3 is fully replaced in the assembled bundle as of Phase 4.** The
  packaged flow now stages Betrayed Alliance (see "Phase 4 bundle" above);
  SQ3 remains only as the *dev fallback* if someone reruns
  `build-package_game.sh` against the SQ3 game/cache paths and re-copies a
  matching ini section — useful for isolating build/bundle mechanics from
  game content, but not part of the shipped configuration. It exists purely
  to verify the build/bundle/cache mechanics end to end; the original
  bundle's shipped ini `description` field said so explicitly (`(dev bundle
  - never deployed)`).

## Gate snapshot — 2026-07-12

Measured against `jon-wasm` at `bc276589bff`, bundle served at
`http://localhost:8080/scummvm.html#sq3`.

**Phase 1 (native-only, `roger_gen_mode=prebuilt`):** clean boot straight
into the SQ3 intro with no picker, rendering visibly native/dithered EGA
(Roger correctly inert). Zero console errors.

**Phase 2 (enhanced, `roger_gen_mode=cache`):**

| Metric | Value |
|---|---|
| Total bundle size | 85 MB |
| `sq3-roger/cache` (generation cache) | 53 MB (62% of bundle) |
| `scummvm.wasm` | 14,007,161 bytes (~14 MB) |
| Cycle period (steady-state walking, n=96) | median 83 ms (range 78–88 ms) |
| Cycle busy time (same sample) | median 4 ms (range 3–14 ms) |

The period/busy figures match the desktop-native healthy baseline (~83 ms
period is healthy; the 225 ms figure from `CLAUDE.md` is the historical
walking-speed regression signature) — no regression signature observed
during real walking in the browser.

Also verified at this snapshot: save/restore and config survive a real page
reload via persisted IDBFS (the generation cache itself is never copied into
IndexedDB — it stays purely network-served from `data/games/<id>-roger/cache/`);
booting with the chronologically-first-loaded cache pair removed from the
index produces no crash, no stall, and no write-failure log spam — the
index-driven filesystem simply skips a request for anything absent from
`index.json` rather than round-tripping to a 404, and generation proceeds
in memory.

**Not yet closed as of this snapshot:** an interactive human soak (F10
display-mode single-step behavior specifically — scripted keyboard events
could not be made to single-step reliably in headless browser automation
and require a real keypress to confirm; plus general extended play in
Chrome and Firefox) was still pending. Treat the figures above as build/perf
evidence, not a substitute for that soak. *(Update, later the same day: the
scripted F10 double-step turned out to be the phantom repeat burst itself —
after the repeat-filter fix `589c114f436` the 2026-07-12 regression sweep
below verifies F10 single-stepping under scripted input; the Chrome+Firefox
human soak remains the open gate.)*

## Performance: packaged vs HTTP-FS — 2026-07-12

The numbers above were measured against the plain-HTTP layout, before
packaging existed. Re-measured the same day against the packaged bundle
(Chrome, 2560x1340) to quantify the fix from "Packaged game data" above.
Columns: **before** = plain-HTTP fallback layout (one XHR per file);
**after** = packaged bundle (`scummvm-game.data` mounted at `/gamedata`).

| Metric | HTTP-FS (before) | Packaged (after) |
|---|---|---|
| First playable room, cold | 20.4 s | ~2.0 s |
| Scene change (Enhanced) | 3.4-4.5 s | <=0.9 s (max 875 ms) |
| Scene change (Original) | 1.5-1.9 s | <=0.8 s (max 770 ms) |
| Intro freezes | 2.0-3.5 s + 871-1118 ms spikes | none >250 ms load-attributable |
| Post-click walk burst | 6-7 cycles @ 240-330 ms | max 19 ms busy |
| Steady walking | 83 ms / 2-5 ms busy | unchanged (healthy) |

The steady-walking row is unchanged by design: packaging only removes
per-file HTTP round-trips from room/scene loads, it does not touch the
per-cycle animate path, so the existing healthy 83 ms baseline (see the
Performance discipline notes in `CLAUDE.md`) was never expected to move.
Every other row was directly caused by synchronous per-file XHRs blocking
the SCI game cycle during loads — packaging collapses those into a single
in-memory-backed preload, which is why the improvement lands almost
entirely on load/scene-change events rather than steady-state play.

## Keyboard input — 2026-07-12

- **Fixed:** F10/F11/F12 (display-mode cycle, diagnostic-log toggle,
  quick-tune panel) were double- or multi-stepping per press. Root cause
  was the Emscripten SDL3 port delivering a burst of five spurious
  `repeat=1` keydown events at an identical timestamp for every real
  keypress. Fix: Roger's hotkey dispatch now ignores key-repeat events
  (commit "SCI: ROGER: Ignore key repeats on debug hotkeys").
- **Fixed (2026-07-12, follow-up):** the same repeat burst also reached
  SCI's own event manager, causing arrow-key walking to start then stop
  after one press, typed letters to arrive in sextuplicate, and parser
  response dialogs to dismiss themselves before they could be read. The
  emscripten event source now drops same-timestamp duplicate keydown and
  text-input events before dispatch (commit "EMSCRIPTEN: Filter phantom
  key repeat bursts"); genuine held-key auto-repeat has advancing
  timestamps and is preserved. Escape now opens and closes the SCI menu
  correctly via keyboard in both display modes.
- **Still open (separate follow-up):** the Enhanced-mode stale
  dropdown-overlay ghost seen when dismissing the menu with the mouse (a
  Roger overlay-invalidation issue, unrelated to input handling).
- **Verified (scripted regression sweep, 2026-07-12):** real same-key-stop
  semantics hold up — two genuine presses of the same key (distinct
  timestamps) both survive the filter (one `ArrowRight` press walks
  continuously across a room and into the next; a second press stops the
  ego immediately, no double-processing). F10 single-steps the
  display-mode cycle exactly once per press across all three modes
  (Enhanced → Original → Side-by-Side → Enhanced); Escape/arrow-key SCI
  menu navigation moves exactly one entry per press in both Enhanced and
  Original, with no stale pixels left on close.
- **New residual, machine-speed only (scripted sweep + DOM census,
  2026-07-12):** under synthetic input faster than any human types
  (~1 ms key hold), a phantom character still slips past the
  same-timestamp filter — `roll` typed as `rolll`, a rapid `abcd` control
  as `abbcd`. A DOM-level keydown/keyup census showed one clean event pair
  per physical press, no `repeat` flag, no duplicate timestamps, so the
  extra character is manufactured inside the SDL3/emscripten layer itself,
  past the exact-equality guards in
  `backends/events/emscriptensdl/emscriptensdl-events.h` (the
  `SDL_EVENT_KEY_DOWN` repeat/scancode/timestamp triple-match, and
  especially the single global last-timestamp slot on the
  `SDL_EVENT_TEXT_INPUT` branch). Real-keyboard typing was user-confirmed
  clean the same day; a fix (timestamp tolerance window, or
  per-character `TEXT_INPUT` dedup) is deferred to a dedicated session.
- **Mouse-menu ghost: not reproduced under synthetic input.** Eight
  varied mousedown/drag/dismiss patterns against the Enhanced-mode menu
  (plus an Original-mode contrast) all came back clean this sweep. The
  ghost above remains filed from real-hardware reports; synthetic-vs-real
  mouse-event timing is the suspected reason it doesn't reproduce under
  scripted input.

## Phase 5 deploy (GitHub Pages)

Phase 5 covers everything this doc's overview scoped out of Phase 4:
hosting, CI publishing, and permanent asset placement. The bundle itself
(Phase 4, above) is unchanged — Phase 5 only adds a dedicated publishing
repo and a workflow that serves it.

## Publishing pipeline (how a change reaches the live demo)

**The mental model, one line:** desktop dev + commit to `jon-wasm` (this
fork) → fast-forward the WSL clone (`~/scummvm-wasm`) → rebuild the wasm
in WSL → assemble the site directory → mirror into the demo repo's
`site/` → push → GitHub Actions publishes to Pages. **The fork holds the
source; the dedicated `roger-web-demo` repo holds the built bundle; CI
(`deploy.yml`) only publishes what's already in `site/` — it never
compiles anything.**

**Three change scenarios** — the commands are the same "Redeploy runbook"
below in every case; only how much gets rebuilt first differs:

- **(A) A Roger / engine code change** (the common case — e.g. the
  subpath data-fetch fix `ef87ff42e8b`, the TEXT_INPUT dedup
  `fd33fa244`/`fa5ec26cdc`): commit to `jon-wasm`, push, then in the WSL
  clone `git fetch origin && git merge --ff-only origin/jon-wasm`, then
  rebuild only the compiled artifacts — `build.sh make` (relinks
  `scummvm.wasm`/`.js`/`.html`) then `build.sh dist` (both carrying
  `--enable-png --enable-freetype2 --enable-zlib`) — re-copy
  `roger-demo.ini`, run `build-assemble_demo.sh`, do the subpath-simulation
  check, robocopy into the demo repo, push, watch, verify. **The game
  data (`scummvm-game.data`) and the art cache are NOT touched** — `dist`
  preserves them; only `scummvm.wasm`/`.js`/`.html` change. This is the
  whole point of the split: a C++ change recompiles the engine, nothing
  else moves.
- **(B) A generation-pipeline change** (a `kTransformVersion` bump or a
  pass-set change — invalidates the cache): FIRST regenerate the desktop
  Roger cache (a normal precache run), then re-run
  `build-package_game.sh` to re-pack the game+cache blob, then continue
  as (A) from `build.sh make`.
- **(C) A game swap** (a different bundled game entirely): re-stage the
  new game's `resource.map`/`resource.001`/license file plus its cache,
  run `build-package_game.sh`, update the shipped ini's
  `path=/gamedata/games/<gameid>` (and section), then proceed as (A).

See "Redeploy runbook" below for the exact per-step commands covering all
three scenarios. `build-info.txt`'s `commit:` line is the deploy's
provenance stamp — it must match the fork commit the served wasm was
actually built from; checking it after a push is the last step of every
redeploy (Redeploy runbook step 9).

### Demo repo

The assembled bundle is published from a **separate, dedicated repo** —
never this fork — per the design spec's §3.1/§4.1 repo-placement rule
(~50 MB of generated assets must never enter this repo's history):

- Repo: `jonborchardt/roger-web-demo`, **public**.
- Live URL: `https://jonborchardt.github.io/roger-web-demo/`.
- Publishing mechanism: `.github/workflows/deploy.yml`, a GitHub Actions
  workflow that publishes the repo's `site/` directory to GitHub Pages on
  every push to `main` (`actions/checkout` → `actions/configure-pages` →
  `actions/upload-pages-artifact` → `actions/deploy-pages`). Pages needed
  **one-time manual enablement** — the first workflow run failed at
  `configure-pages` with `Resource not accessible by integration` (Pages
  had never been turned on for the repo); the fix was a one-shot API call
  before re-running the workflow:

  ```sh
  gh api -X POST repos/jonborchardt/roger-web-demo/pages -f build_type=workflow
  ```

  This only needs to happen once per repo, not per deploy.
- Repo contents: `README.md` (demo description + credits), `COPYING`
  (verbatim GPLv3 text, copied from this fork's `COPYING`),
  `.github/workflows/deploy.yml`, `.gitignore`, and `site/` — the entire
  assembled bundle from `build-assemble_demo.sh` (below), committed and
  replaced wholesale on every redeploy.

**Companion explainer site (added 2026-07-14).** The demo repo's `site/` root is
now a hand-authored explainer site (`index.html` + `comparisons.html` +
`upstreaming.html` + `style.css` + `svg/` + `img/`), committed and edited
*directly in the demo repo* — it is NOT assembled from this fork and does not ride
the redeploy runbook. The compiled game bundle now lives under
`site/betrayed-alliance/`; the runbook's robocopy mirror step targets that folder
so a game redeploy cannot wipe the explainer. The demo repo's `CLAUDE.md` states
which files are hand-authored vs generated. Explainer edits deploy on their own
push to the demo repo's `main` via the existing `deploy.yml`, with no wasm
rebuild.

**Licensing story.** Serving the compiled `scummvm.wasm` to the public is
GPL *distribution*, so the repo carries the standard three-part answer:

1. **`COPYING`** at the repo root — the full GPLv3 text, satisfying the
   "include a copy of the license" obligation for the engine.
2. **`site/build-info.txt`** — the **GPL source pointer**: a small text
   file stamped at assembly time naming the exact fork repo, branch, and
   commit the served binaries were built from:

   ```
   Roger web demo build info
   source: https://github.com/jonborchardt/scummvm (branch jon-wasm)
   commit: ef87ff42e8bd2e530a566263c2e83998fadeb094
   assembled: 2026-07-14T01:40:16Z
   ```

   Since the fork is public, this pointer is a complete-corresponding-source
   answer without shipping a source tarball. `ef87ff42e8b` is the exact
   commit the served `scummvm.wasm`/`scummvm.js` were compiled from (the
   base-relative data-fetch fix below rebuilt them), so the pointer names
   the true build tree.
3. **`site/LICENSE-BetrayedAlliance.txt`** — the bundled fan game's own
   MIT license (copied verbatim from the release's `LICENSE.TXT` at
   assembly time), satisfying the license *condition* attached to
   redistributing Betrayed Alliance's game data alongside the engine.

   The README credits Ryan Slattery / Slattstudio with plain attribution
   only (source repo + `slattstudio.com` links) and an explicit
   "not affiliated with / endorsed by" disclaimer — no partnership framing.

### Assembly script

`dists/emscripten/build-assemble_demo.sh` turns a built `build-emscripten/`
tree into the exact directory that gets mirrored into the demo repo's
`site/`. It is a **tracked fork file**, but because it lives under the
blanket-ignored `dists/` glob it is invisible to a plain `git add` —
**committing it (or any future edit to it) requires `git add -f`.**

The script refuses to assemble a stale or broken bundle — it gates on:

- `scummvm.wasm` and `scummvm-game.data` both present in `build-emscripten/`.
- `scummvm.ini` carries a `[betrayed]` section **and** does not carry a
  `roger_display_mode` override (catches a dev leftover from Variant-A
  side-by-side testing leaking into a release build).
- `scummvm.js` carries the no-fragment `betrayed` boot default (catches a
  stale shell that wasn't actually relinked after a pre-js edit).
- `scummvm-game.data` size falls inside a 20–30 MB window (catches a
  mis-packaged or empty game-data blob for Betrayed Alliance specifically).

Any gate failure is a hard `FAIL: <reason>` to stderr and a non-zero exit
with **no output directory written** — verified directly by a negative
test (appending `roger_display_mode=sbs` to the ini reliably blocked
assembly with no `assembled:` line).

On success it: renames `scummvm.html` → `index.html`; copies the engine
artifacts, `LICENSE-BetrayedAlliance.txt`, favicon/logo/manifest/icons, and
the whole `data/` tree; touches `.nojekyll` (so GitHub Pages doesn't run
Jekyll over the `_`-prefixed emscripten output); and stamps
`build-info.txt` (the GPL source pointer above) with the fork's current
`HEAD` commit and an ISO-8601 assembly timestamp. Output default:
`<repo-root>/demo-site` (gitignored — added to `.gitignore` alongside the
existing `/build*` glob). Last measured output: 30 files, ~54 MB.

### Subpath data fetch (fixed 2026-07-14)

GitHub Pages serves the demo under a subpath (`/roger-web-demo/`, not the
origin root). ScummVM's built-in GUI data directory — `/data/*`: the GUI
theme zips, `fonts.dat`, `translations.dat`, `gui-icons.dat`,
`helpdialog.zip`, `achievements.dat`, `shaders.dat` (**not** any SCI game
or Roger cache data, which ride the subpath-clean `/gamedata` MEMFS mount)
— was fetched from an **absolute** `/data/...` URL, so the browser
resolved it from the origin root and it 404'd under the subpath, even
though those files ship correctly one level down at
`https://jonborchardt.github.io/roger-web-demo/data/...`.

**The visible symptom** was a tiny dialog font: Roger's TTF dialog/header
fonts (`GoMono-Regular.ttf`, `NotoSans-Regular.ttf`, packed in
`data/fonts.dat`) failed to load and fell back to a built-in bitmap font
that tops out ~16 px, so on the hires overlay dialog/narration text
rendered roughly 3-4x too small; the ScummVM save/load dialog also lost
its theme. The game itself always played fine — only `/data/`-backed GUI
assets were affected.

**Fix — commit `ef87ff42e8b`, `backends/fs/emscripten/http-fs.cpp`.** The
HTTP filesystem now seeds its root fetch URL **page-relative** (strips the
leading slash off `DATA_PATH`) so `fetch()` resolves `data/...` against the
document base URL — correct under any subpath *and* at the origin root. The
VFS path stays absolute; only the fetch URL changed. This is a general,
Roger-agnostic portability fix (an absolute `/data/` breaks *any* subpath
deployment, not just this repo's layout) and is a candidate for upstreaming.
Verified on the live public URL: `data/fonts.dat` returns 200, dialog text
renders at full TTF size, and the save/load dialog is themed again.

**The subpath-simulation gotcha still applies to every redeploy.** Serving
the assembled bundle at a plain HTTP root (the desktop dev flow, `python3
-m http.server 8080` inside `build-emscripten/`) does not exercise this
class of bug, because `/data/...` and the server root coincide there.
**Always verify a redeploy candidate by simulating the actual Pages
subpath** (symlink the assembled directory under a path segment and serve
*that*, per the runbook below) — root-only testing will not catch a
subpath-fetch regression.

### Measured public-URL load time

From the live `https://jonborchardt.github.io/roger-web-demo/` (Chrome,
fresh IndexedDB + cleared HTTP cache, cold navigation, 2026-07-13):

| Stage | Time (from navigation start) |
|---|---|
| `scummvm.wasm` fetch complete | ~2.25 s |
| `scummvm-game.data` fetch complete | ~5.07 s (25.7 MB transferred as ~21.1 MB gzip'd over the CDN — the dominant cost) |
| DOMContentLoaded / load | ~0.28 s |
| Title screen fully rendered | **~6–8 s total** |

A same-page reload (after a save, testing IndexedDB persistence) lands in
a comparable ~8–9 s window, with `scummvm.js`/`scummvm-game.js` served
`304` from cache and only `scummvm-game.data` re-fetched. The full
scripted acceptance sweep against this URL passed all 6 checklist items
(cold load with no chrome, network audit, playability round-trip, all
three F10 display modes, save-survives-reload, clean console modulo the
known residuals above) — see `.superpowers/sdd/task-4-report.md`
(local-only, not tracked) for the item-by-item evidence.

### Redeploy runbook

The detailed steps for scenarios A/B/C above, end to end (scenario A —
a pure Roger/engine code change — skips steps 1–2; they only apply when
the cache or packaged game data changed):

1. **If the generation pipeline changed**, regenerate the desktop Roger
   cache first (a normal precache run against the local Betrayed Alliance
   install) — the web bundle only ever ships a *copy* of a cache warmed on
   desktop, it never generates its own.
2. **Re-package the game data + cache** with
   `dists/emscripten/build-package_game.sh`, the same Betrayed Alliance
   invocation used to build the shipped bundle (staging dir holding only
   `resource.map` + `resource.001` + `LICENSE.TXT`, filtered to the
   current cache version/pass token):

   ```sh
   dists/emscripten/build-package_game.sh ~/ba-game-stage \
     '/mnt/j/BetrayedAllianceBook1-v1.3.3.1/sci-fanmade-roger/cache' \
     sci-fanmade v6 p0p2p2p2p2p2p1p0p0p0
   ```

   (See "Phase 4 bundle (Betrayed Alliance)" above for the full staging
   contract and expected file counts. Requires `build.sh dist` to have
   already produced `build-emscripten/` this session.)
3. **Re-copy the shipped ini** — `build.sh dist` does not do this
   automatically, and a fresh `dist` silently reverts `scummvm.ini`:

   ```sh
   cp dists/emscripten/roger-demo.ini build-emscripten/scummvm.ini
   ```
4. **Assemble the site directory:**

   ```sh
   sh dists/emscripten/build-assemble_demo.sh
   ```

   A clean run prints `assembled: <path> (<size>, <N> files)`; any gate
   failure prints `FAIL: <reason>` and writes nothing — treat that as a
   stop, not something to work around.
5. **Subpath-simulation check — do not skip.** Serve the assembled
   directory under a path segment (never bare root) and load it at that
   subpath, exactly mirroring how GitHub Pages will serve it:

   ```sh
   wsl.exe -u jon bash -lc "rm -rf /tmp/pages-sim && mkdir -p /tmp/pages-sim && \
     ln -s /home/jon/scummvm-wasm/demo-site /tmp/pages-sim/roger-web-demo && \
     cd /tmp/pages-sim && python3 -m http.server 8081"
   ```

   then load `http://localhost:8081/roger-web-demo/` (not
   `http://localhost:8081/`) and confirm: cold boot reaches the title
   screen with no chrome, and the only 404s are the known `/data/*` +
   `favicon.ico` residuals documented above — any *new* 404 or a broken
   boot means something regressed and must be fixed before mirroring to
   the demo repo.
6. **Mirror into the demo repo's `site/`:**

   ```powershell
   robocopy "\\wsl.localhost\Ubuntu\home\jon\scummvm-wasm\demo-site" `
     "e:\github2\roger-web-demo\site\betrayed-alliance" /MIR /NFL /NDL
   ```

   `/MIR` is required — it deletes files in `site/` that are no longer in
   the new `demo-site/`, keeping the two in exact sync rather than
   accumulating stale bundle files across redeploys. Robocopy exit codes
   0–7 are success (1 = "files copied", not an error). `/MIR` deletes
   files in the target not in the new bundle — the target is the
   **per-game folder** `site\betrayed-alliance`, NOT `site\` root, so the
   hand-authored explainer at the site root is never touched. (For a new
   game demo, mirror into its own `site\<slug>` folder.)
7. **Commit and push the demo repo** (`e:\github2\roger-web-demo`, not this
   fork) — a normal `git add site` + commit + `git push origin main`.
8. **Watch the deploy workflow:**

   ```sh
   gh run watch <run-id> --repo jonborchardt/roger-web-demo --exit-status
   ```

   (`gh run list --repo jonborchardt/roger-web-demo` finds the run id if
   not run interactively right after the push.)
9. **Public-URL spot check** — confirm the live bundle actually updated
   before calling the redeploy done:

   ```sh
   curl.exe -s https://jonborchardt.github.io/roger-web-demo/betrayed-alliance/build-info.txt
   curl.exe -s -o NUL -w "%{http_code}" https://jonborchardt.github.io/roger-web-demo/betrayed-alliance/scummvm.wasm
   ```

   Confirm `build-info.txt`'s `commit:` line matches the fork commit just
   pushed, and `scummvm.wasm` returns `200`.

### Future direction: move the build into GitHub Actions (retires the separate repo)

The current split — dedicated `roger-web-demo` repo holding the built
bundle, wasm compiled locally in WSL, CI only publishing — is a deliberate
**iterate-fast** choice, not the intended end state. It exists so a Roger
code change can be rebuilt and eyeballed in seconds without waiting on a
heavy emscripten toolchain spin-up in CI on every poke. The manual
robocopy-and-push redeploy is the accepted price of that speed while the
code is still moving.

**Trigger to revisit:** once the code has stabilized ("we like it") and
redeploys become routine rather than exploratory.

**Target end state:** stand up the emscripten build in a GitHub Actions
workflow that compiles the wasm on push and deploys straight to Pages
(`actions/configure-pages` → `upload-pages-artifact` → `deploy-pages`, the
same publish trio, but fed by a CI build step instead of a committed
`site/`). This single move collapses several things at once:

- **No committed binaries anywhere.** CI builds the ~55 MB bundle per run
  and uploads it as the Pages artifact; it never enters any repo's git
  history. Clone size and history stay clean.
- **The separate `roger-web-demo` repo retires.** Its only reason to exist
  is to *hold* the built bundle for Pages to serve; once CI produces the
  bundle on demand, there is nothing to hold. The fork can publish its own
  Pages (`jonborchardt.github.io/scummvm/`-class URL) as the one canonical
  Roger showcase — fitting the fork's role as a long-lived showcase until
  upstream merge.
- **Provenance becomes the workflow run**, not a hand-stamped
  `build-info.txt` — the Pages deployment is tied to the exact source
  commit CI built from, automatically.

**Cost / why it is deferred:** the emscripten build is heavy (emsdk install
+ full wasm link), so a CI round-trip is minutes, not seconds — worth
paying only when manual rebuilds start to chafe, not while iterating. The
`--enable-png --enable-freetype2 --enable-zlib` command-contract
amendment (§ Emscripten build above) must carry into the CI build step, and the
`roger-demo.ini` copy + `build-package_game.sh` game/cache staging (which
`build.sh dist` does not do) must become explicit CI steps — the same gaps
the local runbook already calls out.

This also settles the "pull the demo into the fork vs leave it separate"
question by dissolving it: with CI building and publishing, neither a
separate repo nor a committed-bundle folder is needed — the fork simply
builds and serves itself.

## Phase 6 closeout

Phase 6 is the web-demo effort's closeout: three residuals carried out of
Phases 2.6/4/5 were each investigated to a fix, a partial fix, or a
documented residual, and the design spec that drove Phases 1–5
(`docs/superpowers/specs/2026-07-04-roger-web-demo-design.md`, a
gitignored local-only file) is retired — this document is now the sole
tracked record of the whole effort. Fork tip at closeout: `ec336ab06fa`
on `jon-wasm` (still unmerged; the user merges himself, per project
convention).

### Phase history summary

(Full detail lived in the spec's §9 progress log; recorded here since
the spec is being deleted.)

- **Phase 1** — native-only wasm build boots SQ3 with Roger inert
  (`roger_gen_mode=prebuilt`); verifies the emscripten target compiles
  and runs the unmodified SCI engine.
- **Phase 2** — enhanced mode (`roger_gen_mode=cache`) ships with a
  pre-warmed cache in the bundle; scripted gates (boot, hires plates,
  save/config persistence, display-mode cycling) all passed; steady
  walking measured at the desktop-healthy 83 ms period / 2–5 ms busy.
- **Phase 2.5** — an unplanned interstitial: the user's play test failed
  on *feel* despite green scripted gates. Root-caused to synchronous
  per-file HTTP fetches blocking the SCI cycle on every scene load; fixed
  by packaging game data + cache into a single Emscripten preload blob
  mounted at `/gamedata` (MEMFS) instead of the lazy per-file HTTP
  filesystem — first playable room 20.4 s → ~2 s. Separately, an SDL3
  phantom key-repeat burst (identical-timestamp spurious keydowns) was
  found and filtered, first for Roger's debug hotkeys, then for SCI's own
  event manager.
- **Phase 2.6** — scripted regression sweep closed issues 1a/2/2b/
  3-keyboard (real same-key-stop semantics preserved, no over-filtering).
  Two residuals were filed from this sweep and carried forward into
  Phase 6: the machine-speed TEXT_INPUT phantom-duplication filter edge,
  and the Enhanced-mode mouse-menu overlay ghost (real-hardware-only,
  never reproduced under Playwright).
- **Phase 3** — Betrayed Alliance Book 1 v1.3.3.1 (MIT-licensed, game id
  `sci-fanmade`) validated on desktop. The plan's scripted `.rin`
  playthrough was abandoned by user decision (BA changes rooms only by
  walking the ego off-screen, impractical to script blind), so the user
  played the entire game himself instead — verdict "all good," zero
  rendering anomalies. Zero engine/detection changes were needed for the
  third game; cache grew from a 4,549-file baseline to 4,550 (one missed
  view-cel generated during play, then a permanent hit). A courtesy email
  to the author (Ryan Slattery / Slattstudio) was drafted but never sent
  — sending is the user's call, not part of any Phase.
- **Phase 4** — BA replaced SQ3 as the bundled game: branded landing page
  (`custom_shell.html`), no-fragment boot default so a bare `scummvm.html`
  load boots straight into BA, and Variant B (boot full-screen Enhanced,
  F10 as the discovery path for Side-by-Side) chosen by the user after
  trying both at the wizard room. Measured bundle: ~55 MB total, ~24.5 MB
  `scummvm-game.data` (3 game files + 4,549 cache files), ~14 MB wasm.
- **Phase 5** — deployed to a dedicated public repo,
  `jonborchardt/roger-web-demo`, published to GitHub Pages via
  `.github/workflows/deploy.yml`; live at
  `https://jonborchardt.github.io/roger-web-demo/`. A subpath data-fetch
  bug (ScummVM's built-in GUI data — theme zips/fonts/icons — fetched
  from an absolute `/data/...` URL, breaking under Pages' `/roger-web-demo/`
  subpath) was found post-deploy and fixed (`ef87ff42e8b`), restoring full
  TTF dialog text and the themed save/load dialog on the live subpath.
- **Phase 6** (this section) — closes out the three carried residuals
  (below) and hands off the one remaining human-acceptance pass.

### Per-defect outcomes

**1. Blackletter status-bar title — DOCUMENTED RESIDUAL (fix shipped,
then reverted).** Betrayed Alliance's status/title font repurposes
printable-ASCII code points as blackletter capitals (`$`=B, `#`=A).
Roger's hybrid text path routes printable ASCII to the crisp TTF (only
non-ASCII bytes are blitted from the game's own font glyphs), so the
Enhanced/Side-by-Side status title renders as literal `$etrayed
#lliance`. **Native/Original mode renders it correctly** — it always
draws the game font's own glyph, never the TTF. A fix landed
(`ef0fa7fb762`, "Blit decorative-font status-bar glyphs") that keyed the
full-glyph-blit behavior off the game's ConfMan description string, but
was reverted the same phase (`ec336ab06fa`) because it embedded
game-specific knowledge in `engines/sci/roger`, violating the
game-agnosticism invariant this project holds itself to. A **general,
content-only** fix was investigated and found infeasible: three distinct
pixel metrics were tried and each failed a different way —
  - (a) absolute glyph-vs-reference deviation: a normal `R` and BA's
    decorative `#` scored identically;
  - (b) relative self-vs-best-letter-match across fonts: false-flagged
    ordinary stock-font symbols (`$ / % @ [ \ ] |`);
  - (c) same-font best-own-letter near-duplicate ranking: inverted —
    BA's ornate blackletter `$`/`#` scored *below* genuine stock
    `[`/`]`, which are themselves near-copies of `I`/`l`.

  At SCI0 EGA bitmap resolution (roughly 6–10 px glyphs) no content-only
  signal separates a font's bespoke decorative capitals from ordinary
  symbol/letter resemblances that already exist in stock SCI0 fonts. The
  one approach that would work — diffing each glyph against the
  pristine, unmodified stock SCI0 font — is blocked by this repo's
  no-proprietary-data rule (the stock font is Sierra's and cannot be
  bundled as a test fixture or reference). **Candidate future path:** a
  stock-font-diff detector, if a non-proprietary reference for the stock
  SCI0 font ever becomes available. Accepted as an Enhanced/Side-by-Side-
  mode-only cosmetic residual; Original mode is unaffected.

**2. SDL3 TEXT_INPUT phantom duplicate character — FIXED (partial), with
a residual.** Commits `fd33fa24438` ("Dedup phantom TEXT_INPUT by
content+window") and `fa5ec26cdc1` ("Convert TEXT_INPUT dedup delta
ns->ms"). The emscripten SDL3 port's single global last-timestamp slot
let a machine-speed phantom `SDL_EVENT_TEXT_INPUT` (arriving at a
slightly different timestamp than the real one) slip through the
same-timestamp filter and duplicate a typed character. The fix drops a
`TEXT_INPUT` event when it repeats the same text within a sub-human
30 ms window (the first attempt compared an SDL3 nanosecond timestamp
delta against a millisecond constant — an effective no-op — corrected
in the follow-up commit via `SDL_NS_TO_MS`, with an underflow guard so
out-of-order timestamps fail safe by keeping the event). 30 ms sits
comfortably below the ~60 ms human repeated-key floor, so genuine
double letters (verified with `book`'s double-o at human typing pace)
always survive. This is a real, verified, strictly-safe improvement —
but **not a complete fix**: a phantom that arrives after an intervening
keystroke lands outside any recent-content window and cannot be caught
by this dedup, so some words still show a duplicate character at human
typing pace (observed e.g. `ffrederick`, `ggallahhad` — a duplicated
first character). This residual is real (not synthetic-input-only) but
rare; real-user impact is low. A complete fix needs a different
mechanism — scancode-burst correlation across the whole keydown/textinput
pipeline, not a per-event content/time window — and is out of scope for
this closeout.

**3. Enhanced-mode mouse-menu overlay ghost — DOCUMENTED RESIDUAL
(unreproduced).** Stale Enhanced-mode dropdown pixels reported after
mouse-driven menu use on real hardware; Original mode is clean. Never
reproduced under Playwright across 8+ patterns tried in earlier phases
plus 7 new human-timed patterns tried this phase (title-switch,
row-execute, click-away, drag-off, escape-dismiss, rapid multi-title
cycling, and a click-away contrast), in both Enhanced and Original mode
— every capture came back pixel-identical to a clean baseline (PIL diff).
This remains real-hardware-only. Candidate seam for a future fix:
`engines/sci/roger/overlay/roger_menu_model.{h,cpp}`'s dropdown-close
invalidation path (`onWindowClose(dropdown)` / `onMenuHighlight`) and
the compositor's menu-region invalidation. Note for a future repro
attempt: BA's menu has no press-hold-drag / click-away / drag-off cancel
gesture — only Escape and row-execute/title-switch close it — which
narrows what a real-hardware repro session needs to differ on from the
synthetic attempts already ruled out. Awaits a real-hardware repro
before it can be fixed.

### Human-acceptance checklist (the one remaining gate)

Everything else in Phases 1–6 is scripted-complete and evidenced. The
sole remaining item is a human pass on a second machine, run **once in
Chrome and once in Firefox** (Firefox closes the Phase 2 gate carried
since 2026-07-12):

- Open `https://jonborchardt.github.io/roger-web-demo/` in a fresh
  incognito/private window.
- Confirm it boots straight into Betrayed Alliance with no ScummVM
  launcher/picker/GMM chrome visible.
- Play a few rooms and confirm healthy game speed (no walking-speed
  stutter, no multi-second scene-change freezes).
- Press F10 and confirm it single-steps Enhanced → Original →
  Side-by-Side → Enhanced.
- Save the game, reload the page, restore the save, and confirm it
  returns to the exact saved position.
- Repeat the above in the other browser (Chrome and Firefox both
  required — this is what closes the carried Phase 2 gate).

**On the courtesy email:** a draft to Ryan Slattery / Slattstudio exists
at `docs/superpowers/2026-07-12-ba-courtesy-email.md`. It is the user's
own draft to send or not — **Phase 6 sends nothing and takes no email
action of any kind.**

## Mobile support

Added 2026-07-14 (six commits, `7a3d96bf669`..`9e841fe2f14` on `jon-wasm`):
touch/mobile-keyboard support for the live demo, so a phone or tablet
browser can play Betrayed Alliance without a physical keyboard. This is
additive to everything above — same bundle, same publishing pipeline; only
the shell and two backend files changed.

### `kFeatureVirtualKeyboard` implementation

The Emscripten backend never answered `kFeatureVirtualKeyboard` — SCI calls
`setFeatureState(kFeatureVirtualKeyboard, true)` whenever a text field gets
focus (the name-entry field, the GMM save-name box), and with no backend
support that was a no-op, so mobile browsers never got a chance to show a
keyboard. `backends/platform/sdl/emscripten/emscripten.{h,cpp}` now answers
it: `hasFeature`/`getFeatureState`/`setFeatureState` handle
`kFeatureVirtualKeyboard` alongside the existing `kFeatureFullscreenMode`
case, backed by a new `_virtualKeyboardShown` member and an `EM_JS` shim
(`showMobileKeyboard`) that calls a `window.__mobileKbdShow(show)` function
defined in the shell (below). This mirrors how the Android/iOS ScummVM
backends already answer the same hook — the web backend was the gap.

**This implementation has no Roger dependency and is a clean upstream-PR
candidate** (same category as the `http-fs.cpp` subpath fix in §12) — it is
general SCI/GUI feature parity for the stock Emscripten port, not anything
Betrayed-Alliance- or Roger-specific.

### Hidden-input / text-bridge flow

There is no way to summon a real mobile on-screen keyboard (OSK) without a
focused, editable DOM element, and no way to read what the OSK types except
through that element's `input` events — so `custom_shell.html` carries a
1x1, off-screen, `aria-hidden` `<input id="mobile-kbd-input">`. The flow:

1. SCI calls `setFeatureState(kFeatureVirtualKeyboard, true)` →
   `showMobileKeyboard(true)` (EM_JS) → `window.__mobileKbdShow(true)` →
   the shell clears and focuses `#mobile-kbd-input` (`{ preventScroll: true }`,
   and must run inside/just-after the user gesture that triggered focus, or
   the phone OSK will not appear — a documented mobile-Safari/Chrome
   constraint, not a bug).
2. The phone's real OSK pops (it is now editing a real focused `<input>`)
   and writes into it. The shell listens for `input` events (guarded by
   `compositionstart`/`compositionend` so IME composition — Asian-language
   OSKs — is not diffed mid-composition) and diffs the new value against the
   previous one: a common-prefix scan computes how many trailing characters
   were deleted (each becomes a synthetic Backspace) and which new
   characters were appended (each becomes its own character push). A
   `keydown` listener for `Enter` pushes a Return key directly (OSKs vary in
   whether they fire an `input` event for Enter).
3. Each pushed key calls `Module._EmscriptenKbd_pushKey(keycode, ascii)` — a
   `EMSCRIPTEN_KEEPALIVE` C shim in `emscripten.cpp` — which reaches the live
   `EmscriptenSdlEventSource` via a module-level pointer
   (`g_emscriptenKbdSource`, set/cleared by the event source's own
   constructor/destructor) and calls its new `injectKey(keycode, ascii)`
   method. `injectKey` pushes a keydown+keyup `Common::Event` pair onto a
   new `_injected` queue (`backends/events/emscriptensdl/emscriptensdl-events.h`).
4. `pollEvent()` drains `_injected` before falling through to the normal SDL
   poll, so injected keys are indistinguishable from real ones to the rest
   of the engine — SCI's text-edit widgets, the parser box, and the GMM
   save-name field all just see ordinary key events. **The physical-keyboard
   path is completely unchanged** — this is purely additive.
5. `setFeatureState(kFeatureVirtualKeyboard, false)` → `__mobileKbdShow(false)`
   blurs the hidden input, dismissing the OSK.

A defensive follow-up (`9e841fe2f14`) resets the shell's `composing` flag
whenever the input is (re)focused, so an interrupted IME composition
(navigating away mid-composition, then reopening the keyboard) cannot leave
the flag stuck `true` and silently swallow all subsequent `input` events.

`g_emscriptenKbdSource` is declared `extern` in `emscriptensdl-events.h` and
defined once in `backends/platform/sdl/emscripten/emscripten.cpp`, ensuring
every translation unit shares the single live pointer to the event source.

### Viewport / orientation

`e75839c18cf` adds a `width=device-width, initial-scale=1, user-scalable=no`
viewport meta tag and `touch-action: none` on the canvas, so pinch-zoom and
double-tap-zoom gestures no longer fight the game (both would otherwise
zoom the page instead of being consumed as game taps) and a touch tap maps
1:1 to game coordinates instead of being offset by browser zoom/pan state.

`d6268bbcf75` adds `"orientation": "landscape"` to
`dists/emscripten/assets/manifest.json` (Betrayed Alliance is a landscape
game) and a CSS-only "rotate your phone" hint overlay shown while the
viewport is in portrait. The same commit also avoids forcing fullscreen
during text entry — an iOS Safari quirk where a fullscreen transition can
suppress the on-screen keyboard entirely; harmless to leave off on Android.

### vkeybd fallback (REMOVED 2026-07-15)

**Update 2026-07-15: `--enable-vkeybd` was removed from the build (reconfigured
without it).** Once the native-OSK bridge worked on Android, the built-in
virtual keyboard was only reachable via a 3-finger tap and the user found that
tiny in-engine keyboard useless — the native OS keyboard is now the only
keyboard. The documented build commands above no longer carry the flag; the
`vkeybd_*.zip` packs are no longer bundled. The rest of this section is retained
as history.

`163663bcf8b` adds `--enable-vkeybd` to every documented build command (the
"Emscripten build" command-contract amendment above now lists it alongside
`--enable-png --enable-freetype2 --enable-zlib`). This compiles ScummVM's
own built-in on-screen virtual keyboard into the web-demo build as a
fallback text-input path for any touch device where the native-OSK bridge
above does not apply or does not produce a usable keyboard (e.g. a browser
that blocks programmatic focus outside a stricter gesture window, or a
platform without the DOM `input`-on-hidden-element trick). Bundle wiring is
automatic once the flag is set — `backends/vkeybd/packs/vkeybd_default.zip`
and `vkeybd_small.zip` are staged into `build-emscripten/data/` by
`Makefile.common`'s `DIST_FILES_VKEYBD` whenever `ENABLE_VKEYBD` is defined,
the same way the theme zips and font data are.

**Build-command gotcha specific to this flag:** `dists/emscripten/build.sh`'s
`make` and `dist` tasks do **not** re-run `configure` — only the `configure`
(or `build`, which does all three) task does. Adding `--enable-vkeybd` (or
any new `--enable-*`/`--disable-*` flag) to an existing WSL clone that was
last configured without it requires an explicit
`./dists/emscripten/build.sh configure ...` pass with the new flag before
`make`/`dist` will pick it up — `config.h` silently keeps `#undef
ENABLE_VKEYBD` (or whatever flag) otherwise, with no error, and the vkeybd
packs simply never appear in the assembled bundle.

### Native-OSK verification status

The hidden-input/text-bridge path (the primary mechanism) is
**Android-verified**: confirmed working on a real Android phone in Chrome —
focusing SCI's name field and the GMM save-name box both pop the native
Android keyboard and accepted typed text lands correctly.

**iOS is designed-for but unverified** — no iOS device was available to test
against. The two known iOS Safari quirks (the user-gesture-adjacent-focus
requirement, and fullscreen suppressing the OSK) are both mitigated in the
implementation above (focus happens synchronously from the feature-state
call chain, and fullscreen is not forced during text entry), but neither
mitigation has been confirmed on real iOS hardware. The `--enable-vkeybd`
fallback exists partly as a hedge against this gap. A future iPhone soak is
the open follow-up.

### Android acceptance checklist (human-only gate)

The one remaining gate for mobile support — cannot be automated
(Playwright has no real touch/OSK/orientation-sensor environment). Run on a
real Android phone, in Chrome, at
`https://jonborchardt.github.io/roger-web-demo/`:

1. Boot into BA; at the name prompt, the **phone keyboard appears** and a
   typed name is accepted.
2. Save a game → the save-name box pops the keyboard and accepts a name.
3. Point-and-click plays (tap → walk/look/icon); no pinch/double-tap zoom
   fighting.
4. Landscape is sensible; portrait shows the rotate hint.
5. F10 cycles Enhanced/Original/SBS.

iOS is designed-for but unverified (no device) — the `--enable-vkeybd`
fallback and the non-forced-fullscreen mitigation are the hedges; a future
iPhone soak is the open follow-up (see "Native-OSK verification status"
above).
