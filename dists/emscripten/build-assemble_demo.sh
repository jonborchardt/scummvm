#!/bin/sh
#
# dists/emscripten/build-assemble_demo.sh -- Assembles the deployable Roger web demo site directory
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

# Assemble the deployable Roger web demo site directory from a built
# build-emscripten bundle.
#
# Usage: dists/emscripten/build-assemble_demo.sh [out-dir]
#   out-dir defaults to <repo-root>/demo-site (recreated from scratch).
#
# Gates (refuses to assemble a broken/stale bundle):
#   - scummvm.wasm and scummvm-game.data present
#   - scummvm.ini carries [betrayed] and no roger_display_mode override
#   - scummvm.js carries the no-fragment "betrayed" boot default
#   - scummvm-game.data within the 20-30 MB Betrayed Alliance size window
#
# The output ships scummvm.html renamed to index.html and a build-info.txt
# recording the source commit (the GPL source pointer for the deployed
# binaries).
set -eu

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build-emscripten"
OUT_DIR="${1:-$REPO_ROOT/demo-site}"

fail() { echo "FAIL: $1" >&2; exit 1; }

[ -f "$BUILD_DIR/scummvm.wasm" ] || fail "$BUILD_DIR/scummvm.wasm missing (run build.sh first)"
[ -f "$BUILD_DIR/scummvm-game.data" ] || fail "scummvm-game.data missing (run build-package_game.sh first)"
grep -q '^\[betrayed\]' "$BUILD_DIR/scummvm.ini" || fail "scummvm.ini lacks [betrayed] (re-copy dists/emscripten/roger-demo.ini)"
grep -q 'roger_display_mode' "$BUILD_DIR/scummvm.ini" && fail "scummvm.ini carries roger_display_mode (dev leftover; re-copy dists/emscripten/roger-demo.ini)"
grep -q 'betrayed' "$BUILD_DIR/scummvm.js" || fail "scummvm.js lacks the betrayed boot default (stale shell relink?)"
DATA_SIZE=$(wc -c < "$BUILD_DIR/scummvm-game.data")
[ "$DATA_SIZE" -ge 20000000 ] && [ "$DATA_SIZE" -le 30000000 ] || fail "scummvm-game.data is $DATA_SIZE bytes, outside the 20-30 MB gate"

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"
cp "$BUILD_DIR/scummvm.html" "$OUT_DIR/index.html"
for f in scummvm.js scummvm.wasm scummvm-game.js scummvm-game.data scummvm.ini \
         LICENSE-BetrayedAlliance.txt favicon.ico logo.svg manifest.json \
         scummvm-192.png scummvm-512.png; do
	cp "$BUILD_DIR/$f" "$OUT_DIR/"
done
cp -r "$BUILD_DIR/data" "$OUT_DIR/data"
touch "$OUT_DIR/.nojekyll"
{
	echo "Roger web demo build info"
	echo "source: https://github.com/jonborchardt/scummvm (branch jon-wasm)"
	echo "commit: $(git -C "$REPO_ROOT" rev-parse HEAD)"
	echo "assembled: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
} > "$OUT_DIR/build-info.txt"

echo "assembled: $OUT_DIR ($(du -sh "$OUT_DIR" | cut -f1), $(find "$OUT_DIR" -type f | wc -l) files)"
