#!/bin/bash
#
# add-roger-game.sh -- stage a game and its Roger hi-res art into the Emscripten
# distribution so both are served over HTTP and picked up by ScummVM.
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
# --------------------------------------------------------------------------
# Why this exists
# --------------------------------------------------------------------------
# The Emscripten port serves all game data over HTTP from DATA_PATH ("/data"),
# indexed by build-make_http_index.py -- it does NOT use --preload-file. The
# Roger art provider (engines/sci/roger/) locates art as a sibling of the game
# directory: "<game-dir>/../<game-id>-roger/". To make this work in the browser,
# the game and its "-roger" art directory must sit side by side under
# /data/games/ and be present in index.json.
#
# Run this AFTER `dist-emscripten` (or `build.sh build`) has produced
# ./build-emscripten/.
#
# Usage:
#   ./dists/emscripten/add-roger-game.sh <game-id> <game-dir> <roger-dir>
#
# Example (paths as seen from WSL; J: -> /mnt/j):
#   ./dists/emscripten/add-roger-game.sh sq3 \
#       "/mnt/j/SteamLibrary/steamapps/common/Space Quest Collection/sq3" \
#       "/mnt/j/SteamLibrary/steamapps/common/Space Quest Collection/sq3-roger"
#
# Result (served at the matching /data/... URLs):
#   build-emscripten/data/games/<game-id>/
#   build-emscripten/data/games/<game-id>-roger/
#
# Then register the game in ScummVM with the directory path:
#   /data/games/<game-id>
# (so the provider resolves the art at /data/games/<game-id>-roger/).

set -e

GAME_ID="$1"
GAME_DIR="$2"
ROGER_DIR="$3"

if [[ -z "$GAME_ID" || -z "$GAME_DIR" || -z "$ROGER_DIR" ]]; then
	echo "Usage: $0 <game-id> <game-dir> <roger-dir>" >&2
	exit 1
fi

ROOT_FOLDER=$(pwd)
DATA_DIR="$ROOT_FOLDER/build-emscripten/data"
GAMES_DIR="$DATA_DIR/games"

if [[ ! -d "$GAMES_DIR" ]]; then
	echo "Error: $GAMES_DIR not found. Run 'dist-emscripten' (or build.sh build) first." >&2
	exit 1
fi
if [[ ! -d "$GAME_DIR" ]]; then
	echo "Error: game dir not found: $GAME_DIR" >&2
	exit 1
fi
if [[ ! -d "$ROGER_DIR" ]]; then
	echo "Error: roger art dir not found: $ROGER_DIR" >&2
	exit 1
fi

echo "Staging game '$GAME_ID' -> $GAMES_DIR/$GAME_ID"
rm -rf "${GAMES_DIR:?}/$GAME_ID"
cp -r "$GAME_DIR" "$GAMES_DIR/$GAME_ID"

echo "Staging Roger art    -> $GAMES_DIR/$GAME_ID-roger"
rm -rf "${GAMES_DIR:?}/$GAME_ID-roger"
cp -r "$ROGER_DIR" "$GAMES_DIR/$GAME_ID-roger"

echo "Rebuilding HTTP index for $DATA_DIR"
EMSDK_PYTHON="${EMSDK_PYTHON:-python3}"
"$EMSDK_PYTHON" "$ROOT_FOLDER/dists/emscripten/build-make_http_index.py" "$DATA_DIR"

echo
echo "Done. Register the game in ScummVM with directory path:"
echo "    /data/games/$GAME_ID"
echo "The Roger provider will then resolve art at /data/games/$GAME_ID-roger/."
