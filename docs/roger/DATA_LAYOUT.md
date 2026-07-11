# Roger — On-disk data layout

This document describes what data Roger reads from and writes to disk, where
it lives, and how the cache versioning works.

## What you must supply

Roger requires original game data for the supported games — currently SCI0 EGA
titles:

- **Space Quest III: The Pirates of Pestulon** (DOS)
- **Quest for Glory I EGA** (DOS, also known as Hero's Quest)

These are commercial titles.  Obtain them through a lawful channel such as the
Steam or GOG releases.  The game data is **not** included in this repository.

## Directory layout

Roger does not add any files inside the game data directory.  Instead it
creates a sibling directory named after the ScummVM game id with a `-roger`
suffix.  The sibling sits next to (not inside) the game directory:

```
/games/
    sq3/                   ← game data (resource.map, resource.000, …)
    sq3-roger/             ← Roger working directory (auto-created)
        cache/             ← content-keyed generation cache (see below)
```

The path is derived at runtime from the configured game path stored in
`scummvm.ini`.  No path is hardcoded in the source; the provider resolves the
sibling directory via `gamePath.getParent().appendComponent(gameId + "-roger")`
(see `engines/sci/roger/file_roger_art_provider.cpp:89`).  The directory is
created automatically on first use if it does not exist.

## The generation cache

All hires art is generated in-engine from the SCI resource files.  The output
is written to `<gameid>-roger/cache/` as content-keyed PNG files so that a
repeated visit to the same room loads from disk rather than regenerating.

### Cache file naming

Each PNG filename encodes a full cache key:

```
<gameid>.<transform>.v<version>.<hash>.<passes>.png
```

| Field | Meaning |
|-------|---------|
| `<gameid>` | ScummVM game id (e.g. `sq3`, `qfg1`) |
| `<transform>` | Asset kind (see table below) |
| `v<version>` | Pipeline version (`kTransformVersion`; see Versioning) |
| `<hash>` | 8-hex-digit FNV-1a hash of the source resource bytes |
| `<passes>` | Numeric per-pass encoding of the OMYAC pass list: one `p<n>` token per pass, where `n` is `2` (fill), `1` (line), or `0` (all). The default `affffflaaa` encodes as `p0p2p2p2p2p2p1p0p0p0`. Empty pass list encodes as `none`. (The precache marker file uses the compact character form — `f`/`l`/`a` — via `omyacPassStamp()`; the cache-key PNG filename uses this numeric form.) |

The three transforms are:

| Transform token | Contents |
|-----------------|----------|
| `omyac` | Hires visual plate for a single SCI pic resource |
| `omyacprio` | Priority screen rendered through the OMYAC pipeline (used for sub-pixel sprite occlusion) |
| `scale6x` | Upscaled hires VIEW cel (ego, props, inventory icons) |

The key format is assembled in `RogerAssetGen::cacheKey()` at
`engines/sci/roger/gen/roger_asset_gen.cpp:211–217`.

### Generation modes (`roger_gen_mode`)

| Mode | Behaviour |
|------|-----------|
| `cache` (default) | Load from the cache on a hit; generate and write on a miss |
| `always` | Regenerate and overwrite every time (for pipeline development) |
| `memory` | Generate but never write to disk |
| `prebuilt` | Off-switch: native-only render, no Roger overlay |

### Cache versioning

Cache files are validated by **filename only** — no content checksum is
applied at load time.  A correct filename is treated as a valid cache hit.

`kTransformVersion` is a single integer constant declared at
`engines/sci/roger/gen/roger_asset_gen.h:102` (currently `6`).  It is
incorporated into every cache filename via the `v<version>` field.

**Bump discipline** (enforced by commit policy): any commit that changes the
output of the generation pipeline must bump `kTransformVersion` in the same
commit.  A bit-identical refactor need not bump, but the commit message must
state "output bit-identical; kTransformVersion unchanged" and include a
byte-compare as proof (see `engines/sci/roger/gen/roger_asset_gen.h:90–95`
and the `engines/sci/roger/gen/README.md:25–29`).

Stale files (with an older version number in their name) are **orphaned** in
place, not deleted.  They do not interfere with the newer-keyed files and can
be cleared manually.

Changing the effective OMYAC pass string (`roger_omyac_passes`) also orphans
all plates whose filenames encode the old pass sequence, because the pass
string is part of the key.  The next precache run regenerates them.

### Completed-precache marker

After a full precache run completes, the launcher writes an empty marker file:

```
<gameid>-roger/cache/<gameid>.done.v<version>.<passStamp>.marker
```

The marker name is assembled by `cacheMarkerName()` at
`engines/sci/roger/launcher/roger_picker_model.cpp:27–30`.  Multiple markers
accumulate; switching the pass string back to a previously completed set is
detected as cached immediately.

### Safety

The entire `cache/` directory is a regenerable artifact.  Deleting it causes
the next session to regenerate art on demand (or via the precache button in the
launcher).  The directory is never committed to the repository.

## Screenshots and script captures

Screenshots and `.rin` script captures are written to the directory configured
by the `screenshotpath` key in `scummvm.ini` (read at
`engines/sci/roger/file_roger_art_provider.cpp:943`).  The repository ships a
`screenshots/` directory listed in `.gitignore`; keeping `screenshotpath`
pointed there ensures development captures are never committed.

## What is NOT in the repository

| Item | Where it lives |
|------|----------------|
| Game data (resource files, executables) | Your local game installation |
| Generation cache PNGs | `<gameid>-roger/cache/` alongside your game directory |
| Screenshots and script captures | `screenshots/` or the configured `screenshotpath` |
| Fonts | ScummVM's own `fonts.dat` archive (bundled with ScummVM) |
