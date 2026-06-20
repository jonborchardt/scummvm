# build_and_run.ps1 — Build ScummVM (SCI engine) and launch SQ3
#
# PREREQUISITES: Visual Studio 2019/2022 with "Desktop development with C++" workload.
# Everything else (SDL2, libpng, zlib, etc.) is installed automatically via vcpkg.
#
# FIRST RUN: installs libraries + generates project + full build (~15-30 min)
# SUBSEQUENT RUNS: incremental build + launch (~1-3 min)

param(
    [string]$Config   = "Release",
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

$Root     = $PSScriptRoot
$DistsDir = "$Root\dists\msvc"
$GameDir  = "J:\SteamLibrary\steamapps\common\Space Quest Collection\sq3"
$RogerDir = "J:\SteamLibrary\steamapps\common\Space Quest Collection\roger"

# ── Locate MSBuild via vswhere ────────────────────────────────────────────────
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "Visual Studio not found. Install VS 2019/2022 with C++ workload."
}
$MSBuild = (& $vswhere -latest -requires Microsoft.Component.MSBuild `
    -find "MSBuild\**\Bin\MSBuild.exe") | Select-Object -First 1
if (-not $MSBuild) { Write-Error "MSBuild not found." }
Write-Host "MSBuild: $MSBuild" -ForegroundColor DarkGray

# ── Locate vcpkg ──────────────────────────────────────────────────────────────
# Prefer a git-backed standalone install (so we can get a valid public baseline).
# The VS-bundled vcpkg uses an internal build hash that is not in the public
# GitHub repo, making manifest-mode baseline resolution fail.
$VSRoot   = (& $vswhere -latest -property installationPath)
$vcpkgCmd = Get-Command vcpkg -ErrorAction SilentlyContinue
$vcpkg    = @(
    "C:\vcpkg\vcpkg.exe",
    "$env:VCPKG_ROOT\vcpkg.exe",
    $(if ($vcpkgCmd) { $vcpkgCmd.Source }),
    "$VSRoot\VC\vcpkg\vcpkg.exe"
) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1

if (-not $vcpkg) {
    Write-Host "`nvcpkg not found. Cloning from GitHub..." -ForegroundColor Cyan
    git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
    & C:\vcpkg\bootstrap-vcpkg.bat -disableMetrics
    $vcpkg = "C:\vcpkg\vcpkg.exe"
}
Write-Host "vcpkg:   $vcpkg" -ForegroundColor DarkGray

# ── Load MSVC environment (needed by vcpkg/ninja to find cl.exe) ──────────────
# Prefer vcvarsall.bat (takes arch arg), fall back to vcvars64.bat
$vcvarsall = "$VSRoot\VC\Auxiliary\Build\vcvarsall.bat"
$vcvars64  = "$VSRoot\VC\Auxiliary\Build\vcvars64.bat"
if (Test-Path $vcvarsall) {
    $vcvarscmd = "`"$vcvarsall`" x64"
} elseif (Test-Path $vcvars64) {
    $vcvarscmd = "`"$vcvars64`""
} else {
    Write-Error "Neither vcvarsall.bat nor vcvars64.bat found under $VSRoot\VC\Auxiliary\Build - reinstall VS C++ workload"
}
Write-Host "Loading MSVC environment..." -ForegroundColor DarkGray

# Use a temp .bat file to avoid PowerShell 5.1 parsing && as a statement separator
$bat = [IO.Path]::GetTempFileName() + ".bat"
Set-Content $bat "@echo off`r`ncall $vcvarscmd >nul 2>&1`r`nset" -Encoding ascii
$envLines = cmd /c $bat
Remove-Item $bat -Force
$envLines | ForEach-Object {
    $parts = $_ -split "=", 2
    if ($parts.Length -eq 2) {
        [System.Environment]::SetEnvironmentVariable($parts[0], $parts[1], "Process")
    }
}

# ── Step 1: Install dependencies via vcpkg ────────────────────────────────────
Write-Host "`n[1/4] Installing dependencies via vcpkg (vcpkg.json)..." -ForegroundColor Cyan
Write-Host "      This takes 15-30 min on first run; cached after that." -ForegroundColor DarkGray

# Manifest mode requires a 'builtin-baseline' that is a real public GitHub commit.
# We derive it from the git history of the vcpkg clone we are using and write a
# temporary vcpkg-configuration.json (overrides vcpkg.json's builtin-baseline field).
$vcpkgDir  = Split-Path $vcpkg
$baseline  = (& git -C $vcpkgDir rev-parse HEAD 2>$null)
if ($LASTEXITCODE -ne 0 -or -not $baseline) {
    Write-Error "vcpkg at '$vcpkg' is not a git clone. Delete it and re-run to get a fresh clone from GitHub."
}
$baseline = $baseline.Trim()
Write-Host "  vcpkg baseline: $($baseline.Substring(0,8))..." -ForegroundColor DarkGray
$vcpkgConfig = "$Root\vcpkg-configuration.json"
Set-Content $vcpkgConfig "{`"default-registry`":{`"kind`":`"builtin`",`"baseline`":`"$baseline`"}}" -Encoding utf8

try {
    Push-Location $Root
    & $vcpkg install --triplet x64-windows --overlay-ports="$Root\.github\vcpkg-ports"
    $result = $LASTEXITCODE
    Pop-Location
} finally {
    Remove-Item $vcpkgConfig -ErrorAction SilentlyContinue
}
if ($result -ne 0) { Write-Error "vcpkg install failed." }

# Integrate so MSBuild finds the libraries automatically (no --libraries-path needed)
& $vcpkg integrate install
Write-Host "  Dependencies ready." -ForegroundColor Green

# ── Step 2: Build create_project.exe if needed ────────────────────────────────
$CpSln = "$Root\devtools\create_project\msvc\create_project.sln"
$CpExe = "$Root\devtools\create_project\msvc\x64\Release\create_project.exe"

if (-not (Test-Path $CpExe)) {
    Write-Host "`n[2/4] Building create_project.exe..." -ForegroundColor Cyan
    & $MSBuild $CpSln /p:Configuration=Release /p:Platform=x64 /m /nologo /v:minimal
    if ($LASTEXITCODE -ne 0) { Write-Error "create_project build failed." }
} else {
    Write-Host "[2/4] create_project.exe already built." -ForegroundColor DarkGray
}

# ── Step 3: Generate scummvm.sln if needed ────────────────────────────────────
$Solution = "$DistsDir\scummvm.sln"

if (-not (Test-Path $Solution)) {
    Write-Host "`n[3/4] Generating scummvm.sln (SCI engine only)..." -ForegroundColor Cyan
    Push-Location $DistsDir
    & $CpExe ..\.. --msvc --enable-engine=sci --disable-all-engines
    $result = $LASTEXITCODE
    Pop-Location
    if ($result -ne 0) { Write-Error "Project generation failed." }
    Write-Host "  scummvm.sln generated." -ForegroundColor Green
} else {
    Write-Host "[3/4] scummvm.sln already exists." -ForegroundColor DarkGray
}

# ── Step 4: Build ScummVM ─────────────────────────────────────────────────────
Write-Host "`n[4/4] Building ScummVM ($Config|$Platform)..." -ForegroundColor Cyan
& $MSBuild $Solution /p:Configuration=$Config /p:Platform=$Platform /m /nologo /v:minimal
if ($LASTEXITCODE -ne 0) { Write-Error "ScummVM build failed." }

# ── Find the exe ──────────────────────────────────────────────────────────────
$candidates = @(
    "$DistsDir\$Config\$Platform\scummvm.exe",
    "$DistsDir\$Platform\$Config\scummvm.exe",
    "$DistsDir\$Config\scummvm.exe"
)
$Exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $Exe) {
    Write-Error "scummvm.exe not found. Searched:`n$($candidates -join "`n")"
}

# ── Launch SQ3 ────────────────────────────────────────────────────────────────
Write-Host "`nBuild OK  : $Exe" -ForegroundColor Green
Write-Host "Game      : $GameDir"
Write-Host "Roger art : $RogerDir"
Write-Host ""

if (-not (Test-Path $GameDir)) { Write-Error "Game data not found at: $GameDir" }

Write-Host "Launching SQ3..." -ForegroundColor Green
& $Exe -p $GameDir sq3
