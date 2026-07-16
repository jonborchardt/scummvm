#!/bin/bash
#
# dists/emscripten/build-package_game.sh -- Packages a game's data + Roger cache into a web bundle
#
# ScummVM is the legal property of its developers, whose names
# are too numerous to list here. Please refer to the COPYRIGHT
# file distributed with this source distribution.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

# Package a game directory and its current-key Roger cache into an
# Emscripten preload package (scummvm-game.data + scummvm-game.js).
#
# The package mounts under /gamedata (NOT /data): ScummVM's Emscripten FS
# factory routes /data/* to its per-file synchronous-XHR HTTP filesystem
# unconditionally, so packaged files under /data would never be seen.
# Point the shipped scummvm.ini at path=/gamedata/games/<gameid>.
#
# Never pass --use-preload-cache here: it copies the package into
# IndexedDB, and browser storage must hold saves/config only.
#
# Usage:
#   build-package_game.sh <game-dir> <cache-dir> <gameid> <cache-ver> <passes-token>
# Example:
#   dists/emscripten/build-package_game.sh \
#     "/mnt/j/SteamLibrary/steamapps/common/Space Quest Collection/sq3" \
#     "/mnt/j/SteamLibrary/steamapps/common/Space Quest Collection/sq3-roger/cache" \
#     sq3 v6 p0p2p2p2p2p2p1p0p0p0
set -euo pipefail

GAME_DIR="$1"; CACHE_DIR="$2"; GAMEID="$3"; VER="$4"; PASSES="$5"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
FILE_PACKAGER="${FILE_PACKAGER:-$ROOT/dists/emscripten/emsdk-4.0.10/upstream/emscripten/tools/file_packager.py}"
OUT="$ROOT/build-emscripten"

[ -f "$FILE_PACKAGER" ] || { echo "file_packager not found: $FILE_PACKAGER" >&2; exit 1; }
[ -d "$OUT" ] || { echo "run 'build.sh dist' first ($OUT missing)" >&2; exit 1; }

STAGING="$(mktemp -d)"
trap 'rm -rf "$STAGING"' EXIT

mkdir -p "$STAGING/games/$GAMEID" "$STAGING/games/$GAMEID-roger/cache"
cp -r "$GAME_DIR/." "$STAGING/games/$GAMEID/"
find "$CACHE_DIR" -maxdepth 1 -name "$GAMEID.*.$VER.*.$PASSES.png" \
  -exec cp {} "$STAGING/games/$GAMEID-roger/cache/" \;

GAME_COUNT="$(find "$STAGING/games/$GAMEID" -type f | wc -l)"
CACHE_COUNT="$(find "$STAGING/games/$GAMEID-roger/cache" -type f | wc -l)"
echo "staged: $GAME_COUNT game files, $CACHE_COUNT cache files"
[ "$CACHE_COUNT" -gt 0 ] || { echo "no cache files matched $VER/$PASSES" >&2; exit 1; }

python3 "$FILE_PACKAGER" "$OUT/scummvm-game.data" \
  --preload "$STAGING@/gamedata" \
  --js-output="$OUT/scummvm-game.js"

du -h "$OUT/scummvm-game.data" "$OUT/scummvm-game.js"
