# build_and_run_web.ps1 -- Build ScummVM (SCI) for WebAssembly and serve SQ3 with
# Roger hi-res art. Web counterpart to build_and_run.ps1 (which builds native).
#
# The Emscripten toolchain is bash-only, so this script drives the build inside
# WSL, then serves the result so you open it in your Windows browser. Run it from
# a normal PowerShell prompt:
#
#     .\build_and_run_web.ps1            # full build + stage + serve
#     .\build_and_run_web.ps1 -Fast      # incremental recompile + serve (after a full build)
#
# PREREQUISITE: WSL with a Linux distro (the first full build downloads the
# Emscripten SDK and compiles ScummVM from source -- long. Incremental and -Fast
# runs are much shorter).

param(
    [switch]$Fast,
    [string]$GameId   = "sq3",
    [string]$GameDir  = "J:\SteamLibrary\steamapps\common\Space Quest Collection\sq3",
    [string]$RogerDir = "J:\SteamLibrary\steamapps\common\Space Quest Collection\sq3-roger",
    [int]$Port        = 8080
)

$ErrorActionPreference = "Stop"

# --- WSL availability ---------------------------------------------------------
if (-not (Get-Command wsl -ErrorAction SilentlyContinue)) {
    Write-Error "WSL not found. The Emscripten build requires WSL with a Linux distro."
}

# --- Convert Windows paths to WSL (/mnt/<drive>/...) --------------------------
function ConvertTo-WslPath([string]$p) {
    $s = $p -replace '\\', '/'
    if ($s -match '^([A-Za-z]):/(.*)$') {
        return "/mnt/$($matches[1].ToLower())/$($matches[2])"
    }
    return $s
}

$repoWsl  = ConvertTo-WslPath $PSScriptRoot
$gameWsl  = ConvertTo-WslPath $GameDir
$rogerWsl = ConvertTo-WslPath $RogerDir

Write-Host "Repo (WSL) : $repoWsl"          -ForegroundColor DarkGray
Write-Host "Game (WSL) : $gameWsl"          -ForegroundColor DarkGray
Write-Host "Roger(WSL) : $rogerWsl"         -ForegroundColor DarkGray
Write-Host ""

# --- Build (+ stage, on full builds) inside WSL ------------------------------
if ($Fast) {
    Write-Host "[1/2] Incremental rebuild (emmake make) in WSL..." -ForegroundColor Cyan
    $build = "cd '$repoWsl' && { [ -d build-emscripten ] || { echo 'No build-emscripten/ yet -- run a full build first (omit -Fast).' >&2; exit 1; }; } && ./dists/emscripten/build.sh make && cp scummvm.html scummvm.js scummvm.wasm build-emscripten/"
} else {
    Write-Host "[1/2] Full build + stage game/art in WSL (first run is long)..." -ForegroundColor Cyan
    $build = "cd '$repoWsl' && ./dists/emscripten/build.sh build --disable-all-engines --enable-engine=sci && ./dists/emscripten/add-roger-game.sh '$GameId' '$gameWsl' '$rogerWsl'"
}

& wsl bash -lc $build
if ($LASTEXITCODE -ne 0) { Write-Error "WSL build step failed (exit $LASTEXITCODE)." }

# --- Serve + open in the Windows browser -------------------------------------
$url = "http://localhost:$Port/scummvm.html"
Write-Host ""
Write-Host "[2/2] Serving at $url" -ForegroundColor Green
Write-Host "      In the ScummVM launcher: Add Game -> /data/games/$GameId" -ForegroundColor Green
Write-Host "      Enter the target room; roger-canvas should show the hires art." -ForegroundColor Green
Write-Host "      Toggle in the browser console: Module._roger_set_enabled(0) / (1)" -ForegroundColor Green
Write-Host "      (Ctrl+C to stop the server.)" -ForegroundColor DarkGray
Write-Host ""

Start-Process $url   # opens default Windows browser (refresh once if the server is still starting)
& wsl bash -lc "cd '$repoWsl' && python3 -m http.server $Port --directory build-emscripten"
