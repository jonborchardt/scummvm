# build_and_run.ps1 — Build ScummVM (SCI engine) and launch SQ3
#
# PREREQUISITES: Visual Studio 2019/2022 with "Desktop development with C++" workload.
# Everything else (SDL2, libpng, zlib, etc.) is installed automatically via vcpkg.
#
# FIRST RUN: installs libraries + generates project + full build (~15-30 min)
# SUBSEQUENT RUNS: incremental build + launch (~1-3 min)

param(
    [string]$Config   = "Release",
    [string]$Platform = "x64",
    [switch]$NoLaunch,   # build only; skip launching the game (used for compile verification)
    [string]$Game     = "",   # launch this configured target id (e.g. "qfg1") instead of SQ3;
                              # launches by target so the existing scummvm.ini config (Roger
                              # settings, game path) is used. Empty = default SQ3 via -p path.
    [int]$SaveSlot    = -1,   # auto-load this save slot on startup (ScummVM -x / --save-slot).
                              # e.g. -SaveSlot 1 boots straight into save 001. -1 = no auto-load.
    [switch]$SkipPicker,      # skip the Roger game-picker dialog and boot straight into the
                              # game (or -SaveSlot save). Sets ROGER_NO_LAUNCHER for the launch.
                              # Implied automatically whenever -Game is passed.
    [switch]$Regenerate,      # force-regenerate scummvm.sln (e.g. after changing enabled
                              # features like the event recorder). Deletes the existing solution.
    [string]$Script   = "",   # .rin input script: drive the game automatically, blocking
                              # until the script's `quit` exits it. Captures land in the
                              # game's screenshotpath; log in screenshots\roger-run.log.
    [string]$Live     = "",   # live command file: launch in background, then APPEND .rin
                              # commands to this file to drive the running game one input
                              # at a time (interactive script authoring).
    [switch]$CycleLog,        # per-cycle ROGER-CYCLE telemetry (walking-speed / perf runs)
    [switch]$Diag,            # ROGER-DIAG overlay-state trace for this launch only (sets the
                              # ROGER_DIAG env var; no scummvm.ini edit — ini edits race against
                              # a running instance's config rewrite-on-exit)
    [switch]$Studio,          # launch the Roger Studio tuning environment (ROGER_STUDIO=1,
                              # this launch only; see docs/superpowers/specs/2026-07-02-roger-studio-design.md)
    [switch]$EyeTest,         # TEMPORARY: launch the eye-test genetic pass search
                              # (ROGER_EYETEST=1, this launch only; delete with the tool)
    [ValidateSet("", "enhanced", "original", "sbs")]
    [string]$Mode     = "",   # boot straight into a display mode (F10 still cycles from it):
                              # enhanced (default), original (native), sbs (side-by-side
                              # enhanced|native — the evidence shot for "Roger bug or game
                              # behavior?"). Sets ROGER_DISPLAY_MODE for this launch only.
    [switch]$NoBuild,         # skip dependency install + build entirely and launch the
                              # existing exe (fast .rin-script iteration: seconds, not minutes)
    [int]$TimeoutSec  = 0,    # watchdog for blocking runs: kill scummvm and exit 124 if it
                              # hasn't exited after this many seconds (hung script protection).
                              # 0 = no watchdog.
    [switch]$TruthCap         # evidence mode: .rin captures grab the real overlay pixels
                              # (g_system->grabOverlay) instead of forcing a full clean recompose.
                              # Required to make invalidation faults visible in captures.
)

$ErrorActionPreference = "Stop"

$Root     = $PSScriptRoot
$DistsDir = "$Root\dists\msvc"
$GameDir  = "J:\SteamLibrary\steamapps\common\Space Quest Collection\sq3"
$RogerDir = "J:\SteamLibrary\steamapps\common\Space Quest Collection\roger"

if ($NoBuild) {
    Write-Host "NoBuild set - skipping dependency install and build; launching the existing exe." -ForegroundColor DarkGray
} else {
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

    # Classic-mode install into the vcpkg root. The scummvm.vcxproj generated by
    # create_project has no vcpkg manifest props, so it relies on the global MSBuild
    # integration (`vcpkg integrate install`), which looks in <vcpkgroot>\installed\.
    # Manifest mode would install into .\vcpkg_installed\ (invisible to that
    # integration) and also requires a public-commit baseline, so we avoid it here.
    $deps = (Get-Content "$Root\vcpkg.json" -Raw | ConvertFrom-Json).dependencies |
        ForEach-Object { "$($_):x64-windows" }

    # Run from a manifest-free directory so vcpkg uses classic mode (a vcpkg.json in
    # the cwd would force manifest mode, which rejects explicit package arguments).
    $vcpkgDir = Split-Path $vcpkg
    Push-Location $vcpkgDir
    & $vcpkg install @deps --overlay-ports="$Root\.github\vcpkg-ports"
    $result = $LASTEXITCODE
    Pop-Location
    if ($result -ne 0) { Write-Error "vcpkg install failed." }

    # Integrate so MSBuild applocal-copies the runtime DLLs next to the exe.
    & $vcpkg integrate install

    # Point ScummVM's generated .props at the vcpkg installed tree. They reference
    # $(SCUMMVM_LIBS)\include, \include\SDL2 and \lib — which is exactly the vcpkg
    # x64-windows layout. Without this, <SDL.h> (in include\SDL2\) isn't found.
    $env:SCUMMVM_LIBS = "$vcpkgDir\installed\x64-windows"
    Write-Host "  SCUMMVM_LIBS: $env:SCUMMVM_LIBS" -ForegroundColor DarkGray

    # create_project hardcodes the library names from the official ScummVM deps
    # bundle. A few vcpkg packages produce a differently-named import lib, so make
    # aliases the linker can find. Map: <scummvm-expected> = <vcpkg-actual>.
    $libDir = "$env:SCUMMVM_LIBS\lib"
    $libAliases = @{
        "zlib.lib"       = "z.lib"
        "fluidsynth.lib" = "libfluidsynth-3.lib"
    }
    foreach ($want in $libAliases.Keys) {
        $have = Join-Path $libDir $libAliases[$want]
        $dest = Join-Path $libDir $want
        if ((Test-Path $have) -and -not (Test-Path $dest)) {
            Copy-Item $have $dest
            Write-Host "  aliased $($libAliases[$want]) -> $want" -ForegroundColor DarkGray
        }
    }
    Write-Host "  Dependencies ready." -ForegroundColor Green

    # ── Step 2: Build create_project.exe if needed ────────────────────────────────
    $CpSln = "$Root\devtools\create_project\msvc\create_project.sln"
    $CpExe = "$Root\devtools\create_project\msvc\Release\create_project.exe"

    if (-not (Test-Path $CpExe)) {
        Write-Host "`n[2/4] Building create_project.exe..." -ForegroundColor Cyan
        & $MSBuild $CpSln /p:Configuration=Release /p:Platform=Win32 /m /nologo /v:minimal
        if ($LASTEXITCODE -ne 0) { Write-Error "create_project build failed." }
    } else {
        Write-Host "[2/4] create_project.exe already built." -ForegroundColor DarkGray
    }

    # ── Step 3: Generate scummvm.sln if needed ────────────────────────────────────
    $Solution = "$DistsDir\scummvm.sln"

    if ($Regenerate -and (Test-Path $Solution)) {
        Write-Host "  -Regenerate: removing existing scummvm.sln to force a fresh project gen." -ForegroundColor DarkGray
        Remove-Item $Solution -Force
    }

    if (-not (Test-Path $Solution)) {
        Write-Host "`n[3/4] Generating scummvm.sln (SCI engine only, event recorder enabled)..." -ForegroundColor Cyan
        Push-Location $DistsDir
        # Order matters: --disable-all-engines must come BEFORE --enable-engine=sci,
        # otherwise it disables SCI again and no ENABLE_SCI define is emitted.
        # --enable-eventrecorder: ENABLE_EVENTRECORDER, so --record-mode=record/fast_playback
        # work — used by roger_spike.ps1 to drive QFG1 to the bugged town deterministically
        # (record once, replay headlessly forever) without manual play.
        & $CpExe ..\.. --msvc --disable-all-engines --enable-engine=sci --enable-eventrecorder
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
    # exit, don't just Write-Error: falling through here used to LAUNCH the stale exe
    # after a failed build (which then held the exe lock and made the next build fail too).
    if ($LASTEXITCODE -ne 0) { Write-Error "ScummVM build failed."; exit 1 }
}

# ── Find the exe ──────────────────────────────────────────────────────────────
$candidates = @(
    "$DistsDir\$Config$Platform\scummvm.exe",
    "$DistsDir\$Config\$Platform\scummvm.exe",
    "$DistsDir\$Platform\$Config\scummvm.exe",
    "$DistsDir\$Config\scummvm.exe"
)
$Exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $Exe) {
    # Fall back to a recursive search so an unexpected layout still launches.
    $Exe = Get-ChildItem -Path $DistsDir -Recurse -Filter scummvm.exe -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $Exe) {
    Write-Error "scummvm.exe not found. Searched:`n$($candidates -join "`n")"
}

# ── Deploy ScummVM data files next to the exe ─────────────────────────────────
# create_project's MSVC project deploys the vcpkg DLLs (applocal) but NOT ScummVM's
# own data files. fonts.dat is required for TTF text (Roger hires dialogs); without
# it loadTTFFontFromArchive fails and text falls back to a tiny bitmap font. Copy
# the engine-data .dat files and the GUI theme .dat files next to the exe so they
# are on the runtime search path.
$ExeDir = Split-Path $Exe
$dataSrc = @(
    "$Root\dists\engine-data\fonts.dat",
    "$Root\gui\themes\gui-icons.dat",
    "$Root\gui\themes\translations.dat",
    "$Root\gui\themes\shaders.dat"
)
foreach ($d in $dataSrc) {
    if (Test-Path $d) {
        $dest = Join-Path $ExeDir (Split-Path $d -Leaf)
        if (-not (Test-Path $dest) -or (Get-Item $d).LastWriteTime -gt (Get-Item $dest).LastWriteTime) {
            Copy-Item $d $dest -Force
            Write-Host "  deployed $(Split-Path $d -Leaf)" -ForegroundColor DarkGray
        }
    }
}

# ── Launch SQ3 ────────────────────────────────────────────────────────────────
Write-Host "`nBuild OK  : $Exe" -ForegroundColor Green
Write-Host "Game      : $GameDir"
Write-Host "Roger art : $RogerDir"
Write-Host ""

if ($NoLaunch) {
    Write-Host "NoLaunch set - build only, skipping SQ3 launch." -ForegroundColor DarkGray
    return
}

# Build the save-slot argument shared by both launch paths.
$saveArgs = @()
if ($SaveSlot -ge 0) { $saveArgs = @("--save-slot=$SaveSlot") }

# ── Input automation (Roger verification loop) ───────────────────────────────
# Env-first knobs (per-process, never touch scummvm.ini) read by
# FileRogerArtProvider; see docs/roger.md. Clear stale values first so a
# previous run in this shell can't leak automation into a manual launch.
foreach ($v in "ROGER_INPUT_SCRIPT", "ROGER_INPUT_LIVE", "ROGER_CYCLE_LOG", "ROGER_NO_LAUNCHER", "ROGER_DISPLAY_MODE", "ROGER_DIAG", "ROGER_STUDIO", "ROGER_EYETEST") {
    Remove-Item "Env:$v" -ErrorAction SilentlyContinue
}
$logArgs = @()
if ($Script -or $Live -or $CycleLog -or $Diag -or $Studio -or $EyeTest) {
    $shots = "$Root\screenshots"
    if (-not (Test-Path $shots)) { New-Item -ItemType Directory -Force $shots | Out-Null }
    $logArgs = @("--logfile=$shots\roger-run.log")
}
if ($Script) {
    if (-not (Test-Path $Script)) { Write-Error "Input script not found: $Script" }
    $env:ROGER_INPUT_SCRIPT = (Resolve-Path $Script).Path
    $env:ROGER_NO_LAUNCHER = "1"   # automation is deterministic; never show the picker
    Write-Host "Input script: $($env:ROGER_INPUT_SCRIPT)" -ForegroundColor Cyan
    # Fresh evidence: remove the previous run log and this script's labelled
    # captures, so a stale artifact can never be read as this run's result.
    Remove-Item "$shots\roger-run.log" -ErrorAction SilentlyContinue
    Select-String -Path $Script -Pattern '^\s*(capture|snap)\s+(\S+)' | ForEach-Object {
        Remove-Item "$shots\roger-*-$($_.Matches[0].Groups[2].Value)-*.png" -ErrorAction SilentlyContinue
    }
}
if ($Live) {
    if (-not (Test-Path $Live)) { New-Item -ItemType File -Force $Live | Out-Null }
    $env:ROGER_INPUT_LIVE = (Resolve-Path $Live).Path
    $env:ROGER_NO_LAUNCHER = "1"
    Write-Host "Live command file: $($env:ROGER_INPUT_LIVE) (append .rin lines to drive)" -ForegroundColor Cyan
}
if ($CycleLog) {
    $env:ROGER_CYCLE_LOG = "1"
    Write-Host "Cycle telemetry: ROGER-CYCLE lines in screenshots\roger-run.log" -ForegroundColor Cyan
}
if ($Diag) {
    $env:ROGER_DIAG = "1"
    Write-Host "Diag trace: ROGER-DIAG lines in screenshots\roger-run.log (this launch only)" -ForegroundColor Cyan
}
if ($TruthCap) {
    $env:ROGER_TRUTH_CAPTURE = "1"
    Write-Host "Truth captures: .rin captures read the presented frame, not a forced full recompose" -ForegroundColor Cyan
}
if ($Studio) {
    $env:ROGER_STUDIO = "1"
    Write-Host "Roger Studio: tuning environment (this launch only)" -ForegroundColor Cyan
}
if ($EyeTest) {
    $env:ROGER_EYETEST = "1"
    Write-Host "Roger EyeTest: genetic pass search on pic 2 (this launch only)" -ForegroundColor Cyan
}
if ($Mode) {
    $env:ROGER_DISPLAY_MODE = $Mode
    Write-Host "Display mode: $Mode (this launch only)" -ForegroundColor Cyan
}

# Scripted runs: assert/fail inside the .rin logs a "ROGER-SCRIPT: FAIL" marker.
# Surface it as exit 125 so a looping caller can branch on the exit code alone
# (124 = watchdog timeout, 125 = scripted assertion failure, else = game exit).
function Test-ScriptFail {
    if (-not $Script) { return $false }
    $log = "$Root\screenshots\roger-run.log"
    return (Test-Path $log) -and (Select-String -Path $log -Pattern 'ROGER-SCRIPT: FAIL' -Quiet)
}

# Boot straight into the game, bypassing the Roger picker dialog, when either
# -SkipPicker is set OR a specific -Game target was passed (a named target means
# the caller already knows what to launch, so the picker is just in the way).
# ROGER_NO_LAUNCHER is read by getenv() in engines/sci/sci.cpp; it's per-process
# (this launch only) so it never touches scummvm.ini.
if ($SkipPicker -or $Game) {
    $env:ROGER_NO_LAUNCHER = "1"
    Write-Host "Bypassing the Roger game-picker dialog (SkipPicker or -Game)." -ForegroundColor DarkGray
}

if ($Game) {
    # Launch a configured target by id (uses scummvm.ini: game path + Roger settings).
    if ($SaveSlot -ge 0) {
        Write-Host "Launching target '$Game' (auto-loading save slot $SaveSlot)..." -ForegroundColor Green
    } else {
        Write-Host "Launching target '$Game'..." -ForegroundColor Green
    }
    $gameArgs = $logArgs + $saveArgs + @($Game)
} else {
    if (-not (Test-Path $GameDir)) { Write-Error "Game data not found at: $GameDir" }
    if ($SaveSlot -ge 0) {
        Write-Host "Launching SQ3 (auto-loading save slot $SaveSlot)..." -ForegroundColor Green
    } else {
        Write-Host "Launching SQ3..." -ForegroundColor Green
    }
    $gameArgs = $logArgs + @("-p", $GameDir) + $saveArgs + @("sq3")
}

# Start-Process joins -ArgumentList with spaces before CreateProcess, so quote
# any element containing one (game path, logfile path) for those launch modes.
$quotedArgs = $gameArgs | ForEach-Object { if ($_ -match " ") { "`"$_`"" } else { $_ } }

if ($Live) {
    $p = Start-Process -FilePath $Exe -ArgumentList $quotedArgs -PassThru
    Write-Host "Running in background (PID $($p.Id)). Append commands to $Live; 'quit' line exits." -ForegroundColor Cyan
} elseif ($TimeoutSec -gt 0) {
    # Watchdog: a hung script (undismissed dialog, missing `quit`) must not hang
    # the caller; kill and exit 124 so timeout is distinguishable from game exit.
    $p = Start-Process -FilePath $Exe -ArgumentList $quotedArgs -PassThru
    if (-not $p.WaitForExit($TimeoutSec * 1000)) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        Write-Host "TIMEOUT: run exceeded ${TimeoutSec}s; killed scummvm (PID $($p.Id)). Hung script? See screenshots\roger-run.log" -ForegroundColor Red
        exit 124
    }
    if (Test-ScriptFail) {
        Write-Host "SCRIPT FAIL: assert/fail marker in screenshots\roger-run.log" -ForegroundColor Red
        exit 125
    }
    Write-Host "Game exited (code $($p.ExitCode))." -ForegroundColor DarkGray
    exit $p.ExitCode
} else {
    & $Exe @gameArgs
    $code = $LASTEXITCODE
    if (Test-ScriptFail) {
        Write-Host "SCRIPT FAIL: assert/fail marker in screenshots\roger-run.log" -ForegroundColor Red
        exit 125
    }
    exit $code
}
