# Web Demo — Emscripten Build and Bundle

**Downstream-only.** This document, the WSL/emscripten build flow it describes,
and the dev bundle it produces are fork-maintenance material — none of it is
part of any upstream PR. Upstream ScummVM's own emscripten port is unaffected;
Roger's web demo is a downstream packaging exercise on top of it.

This is a factual build/run record, re-derivable from a clean WSL install. It
does not cover deployment (hosting, permanent asset placement, the Betrayed
Alliance bundle) — that is Phase 4/5 territory and belongs in a later doc.

## Overview

Roger's native target is Windows/MSVC (see the top-level `CLAUDE.md`). The web
demo is a **second, WSL-hosted build** of the same source tree targeting
emscripten/wasm, produced from a dedicated Linux-side clone so the Windows
checkout and its build state are never touched. The flow has four stages:

1. WSL clone of the Windows repo, on its own branch (`jon-wasm`).
2. A native Linux build + `make test`, as a portability check before touching
   emscripten (catches MSVC-only assumptions early, cheaply).
3. An emscripten build of the same source.
4. A `dist` bundle: engine + game data + a warmed Roger generation cache,
   served locally over plain HTTP for browser verification.

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

Game data is copied in from a real local install (never committed):

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
path=/data/games/sq3
```

**Enhanced mode (Phase 2, cache shipped in the bundle):**

Identical except `roger_gen_mode=cache`. This is the only line that
changes between the two variants.

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
a 404. Regenerate it after **any** change to bundle contents (adding game
data, ini edits, or cache files):

```sh
python3 dists/emscripten/build-make_http_index.py build-emscripten/data
```

This writes/updates `index.json` at every directory level that needs one,
including `data/games/<gameid>-roger/cache/index.json` (the file the Roger
provider actually reads to know what's cached).

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
- **SQ3 bundle is dev-only and never deployed.** It exists purely to verify
  the build/bundle/cache mechanics end to end; the shipped ini's
  `description` field says so explicitly (`(dev bundle - never deployed)`).
  A permanent public deployment is Phase 4/5 territory with its own game
  (Betrayed Alliance) and hosting plan, not covered here.

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
evidence, not a substitute for that soak.
