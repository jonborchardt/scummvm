#!/bin/bash
#
# build_and_run_web.sh -- Build ScummVM (SCI engine) for WebAssembly and serve
# SQ3 with Roger hi-res art, then open it in a browser.
#
# This is the web counterpart to build_and_run.ps1 (which builds the NATIVE
# Windows binary). Run it from a Unix shell -- on Windows that means WSL:
#
#     wsl ./build_and_run_web.sh            # from a PowerShell prompt
#     ./build_and_run_web.sh                # from inside a WSL shell
#
# First run is long: it downloads the Emscripten SDK and compiles ScummVM +
# any 3rd-party libs from source. Subsequent runs are incremental.
#
# Paths default to the Steam install (J: -> /mnt/j under WSL); override via env:
#     GAME_ID=sq3 GAME_DIR=/path/to/sq3 ROGER_DIR=/path/to/sq3-roger ./build_and_run_web.sh
#
# PORT controls the dev server (default 8080). On WSL2, http://localhost:PORT
# is reachable from the Windows browser.

set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

GAME_ID="${GAME_ID:-sq3}"
GAME_DIR="${GAME_DIR:-/mnt/j/SteamLibrary/steamapps/common/Space Quest Collection/sq3}"
ROGER_DIR="${ROGER_DIR:-/mnt/j/SteamLibrary/steamapps/common/Space Quest Collection/sq3-roger}"
PORT="${PORT:-8080}"

echo "==> [1/3] Building ScummVM (SCI) for WebAssembly"
./dists/emscripten/build.sh build --disable-all-engines --enable-engine=sci

echo "==> [2/3] Staging '$GAME_ID' + Roger art into the served data dir"
./dists/emscripten/add-roger-game.sh "$GAME_ID" "$GAME_DIR" "$ROGER_DIR"

echo "==> [3/3] Serving build-emscripten/ at http://localhost:$PORT/scummvm.html"
echo "    In the ScummVM launcher: Add Game -> /data/games/$GAME_ID"
echo "    Then enter the target room; the roger-canvas should show the hires art."
echo "    Toggle in the browser console: Module._roger_set_enabled(0) / (1)"
echo
echo "    (Ctrl+C to stop the server.)"
exec python3 -m http.server "$PORT" --directory "$ROOT/build-emscripten"
