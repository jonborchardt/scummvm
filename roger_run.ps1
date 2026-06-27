# roger_run.ps1 — Launch SQ3 (Roger overlay), auto-load a save, and capture the
# composited hires scene to PNG for verification (Stage 0 / H1 of the Roger plan).
#
# HOW CAPTURE WORKS (and why it is NOT keystrokes):
#   Injected Alt+s (screenshot) and F10 (overlay toggle) never reach SDL — both are
#   Win32 menu-activation keys, so an injected Alt/F10 puts the window in menu-mode
#   and the key is swallowed before SDL sees it (verified: injected Alt+Enter did not
#   toggle fullscreen even with confirmed foreground). Instead we set the engine flag
#   `roger_autoshot=true`, which makes the Roger provider dump the composited scene to
#   <screenshotpath>/roger-<picId>-overlay.png on the first frame of each room. No
#   keystrokes, focus, or foreground required.
#
#   `roger_autoshot` is a ConfMan key and ScummVM rejects unknown CLI options, so we
#   can't pass it on the command line. We instead launch with --config pointing at a
#   THROWAWAY COPY of the user's scummvm.ini that has roger_autoshot=true added to
#   [scummvm]; the real config is never touched.
#
# Examples:
#   .\roger_run.ps1                 # slot 1 (room2): dump room-2 overlay PNG, then close
#   .\roger_run.ps1 -KeepOpen       # leave the game running after capture
#   .\roger_run.ps1 -SaveSlot 1 -WaitMs 12000

param(
    [int]$SaveSlot   = 1,                 # ScummVM save slot to auto-load (room2 = slot 1)
    [string]$ShotDir = "",                # screenshot + log + temp-config dir (default: scratchpad)
    [int]$WaitMs     = 10000,             # ms to let the window appear, save load, and first frame render
    [switch]$KeepOpen                     # do not close the game after capturing
)

$ErrorActionPreference = "Stop"

$Root    = $PSScriptRoot
$GameDir = "J:\SteamLibrary\steamapps\common\Space Quest Collection\sq3"
$UserIni = "$env:APPDATA\ScummVM\scummvm.ini"

if ([string]::IsNullOrEmpty($ShotDir)) {
    $ShotDir = "C:\Users\Jon\AppData\Local\Temp\claude\e--github2-scummvm\02eb1b21-3cea-41f2-83af-1d659d071676\scratchpad"
}
New-Item -ItemType Directory -Force -Path $ShotDir | Out-Null
$LogFile  = Join-Path $ShotDir "scummvm.log"
$CfgCopy  = Join-Path $ShotDir "scummvm-roger.ini"

# ── Locate the built scummvm.exe (same search order as build_and_run.ps1) ──────
$DistsDir = "$Root\dists\msvc"
$candidates = @(
    "$DistsDir\Releasex64\scummvm.exe",
    "$DistsDir\Release\x64\scummvm.exe",
    "$DistsDir\x64\Release\scummvm.exe",
    "$DistsDir\Release\scummvm.exe"
)
$Exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $Exe) {
    $Exe = Get-ChildItem -Path $DistsDir -Recurse -Filter scummvm.exe -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $Exe)               { Write-Error "scummvm.exe not found under $DistsDir. Build first: .\build_and_run.ps1 -NoLaunch" }
if (-not (Test-Path $GameDir)) { Write-Error "Game data not found at: $GameDir" }
if (-not (Test-Path $UserIni)) { Write-Error "scummvm.ini not found at: $UserIni" }

# ── Build a throwaway config copy with roger_autoshot=true in [scummvm] ────────
# We do NOT mutate the user's real ini; the engine reads roger_autoshot from this copy.
$ini = Get-Content $UserIni -Raw
if ($ini -match '(?m)^\s*roger_autoshot\s*=') {
    $ini = $ini -replace '(?m)^\s*roger_autoshot\s*=.*$', 'roger_autoshot=true'
} elseif ($ini -match '(?m)^\[scummvm\]\s*$') {
    $ini = $ini -replace '(?m)^\[scummvm\]\s*$', "[scummvm]`r`nroger_autoshot=true"
} else {
    $ini = "[scummvm]`r`nroger_autoshot=true`r`n" + $ini
}
Set-Content -Path $CfgCopy -Value $ini -Encoding UTF8

Write-Host "Exe        : $Exe"      -ForegroundColor DarkGray
Write-Host "Game       : $GameDir"  -ForegroundColor DarkGray
Write-Host "Save slot  : $SaveSlot" -ForegroundColor DarkGray
Write-Host "Config copy: $CfgCopy (roger_autoshot=true)" -ForegroundColor DarkGray
Write-Host "Shot/log   : $ShotDir"  -ForegroundColor DarkGray

# Remove stale autoshot PNGs so we report only this run's output.
Get-ChildItem $ShotDir -Filter "roger-*.png" -ErrorAction SilentlyContinue | Remove-Item -Force

# ── Launch the game ────────────────────────────────────────────────────────────
# NOTE: hand-build the arg string with every path quoted. PowerShell 5.1's
# Start-Process -ArgumentList (array form) does NOT quote elements, so a path with a
# space (e.g. "...\Space Quest Collection\sq3") would be split and rejected by -p.
$argLine = "--config=`"$CfgCopy`" -p `"$GameDir`" --save-slot=$SaveSlot --screenshotpath=`"$ShotDir`" --logfile=`"$LogFile`" sq3"
Write-Host "`nLaunching: scummvm $argLine" -ForegroundColor Green
$proc = Start-Process -FilePath $Exe -ArgumentList $argLine -PassThru
Write-Host "Launched PID $($proc.Id); waiting ${WaitMs}ms for save load + first frame..." -ForegroundColor DarkGray
Start-Sleep -Milliseconds $WaitMs

# ── Report the captured PNG(s) ─────────────────────────────────────────────────
$shots = Get-ChildItem $ShotDir -Filter "roger-*.png" -ErrorAction SilentlyContinue | Sort-Object Name
Write-Host ("-" * 70)
if ($shots.Count -gt 0) {
    Write-Host "AUTOSHOT PNGs:" -ForegroundColor Green
    $shots | ForEach-Object { Write-Host "  $($_.FullName) ($($_.Length) bytes)" }
} else {
    Write-Host "No autoshot PNGs were produced." -ForegroundColor Red
    Write-Host "Check: did the save load room 2? is roger_autoshot read? see the log below." -ForegroundColor Yellow
}
Write-Host "Logfile: $LogFile"
if (Test-Path $LogFile) {
    Write-Host "--- log tail ---" -ForegroundColor DarkGray
    Get-Content $LogFile -Tail 15
}
Write-Host ("-" * 70)

# ── Clean up unless asked to keep the game open ────────────────────────────────
if (-not $KeepOpen) {
    if (-not $proc.HasExited) {
        Write-Host "Closing game (PID $($proc.Id))..." -ForegroundColor DarkGray
        try { $proc.CloseMainWindow() | Out-Null; Start-Sleep -Milliseconds 800 } catch {}
        if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }
    }
} else {
    Write-Host "KeepOpen set - leaving the game running (PID $($proc.Id))." -ForegroundColor DarkGray
}
