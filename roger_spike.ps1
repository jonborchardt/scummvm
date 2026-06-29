# roger_spike.ps1 -- self-driving QFG1 fresh-start repro + root-cause validation.
#
# Purpose: drive QFG1 from a fresh game start through the intro + character
# creation into the town (pic 300) DETERMINISTICALLY and capture the roger_diag
# trace, with NO manual play. Uses ScummVM's event recorder:
#
#   1) ONE-TIME:  .\roger_spike.ps1 -Record
#        Play the intro -> town once and quit. ScummVM records every input +
#        RNG + timing into a replayable recording. This is the only manual run.
#
#   2) FOREVER:   .\roger_spike.ps1
#        Replays that recording with --record-mode=fast_playback (auto-exits at
#        the end), captures scummvm.log, and validates the root-cause signature:
#        reached pic 300, how many addToPic statics were captured (0 = the bug,
#        the "missing views"), and whether Feeder B stamped native regions.
#
# Requires a build with the event recorder enabled:
#   .\build_and_run.ps1 -Regenerate -NoLaunch
#
param(
    [switch]$Record,                          # one-time manual recording run
    [string]$Name   = "qfg1-freshstart-town", # recording name (--record-file-name)
    [string]$Game   = "qfg1",                 # configured target id (uses its scummvm.ini)
    [switch]$Headless,                        # playback with dummy SDL drivers (faster, no window)
    [int]$ExpectPic = 300                     # the town pic the validation checks
)
$ErrorActionPreference = "Stop"

$Root = $PSScriptRoot
$Exe  = "$Root\dists\msvc\Releasex64\scummvm.exe"
$Log  = "$env:APPDATA\ScummVM\Logs\scummvm.log"
$Out  = "$Root\screenshots\spike"
New-Item -ItemType Directory -Force $Out | Out-Null

if (-not (Test-Path $Exe)) {
    Write-Error "scummvm.exe not found at $Exe - build first: .\build_and_run.ps1 -NoLaunch"
}

# The build must have the event recorder compiled in (--enable-eventrecorder).
$hasRec = (& $Exe --help 2>&1 | Select-String -Pattern "record-mode")
if (-not $hasRec) {
    Write-Error "This scummvm.exe has no event recorder. Rebuild: .\build_and_run.ps1 -Regenerate -NoLaunch"
}

# Set roger_gen_mode within the [qfg1] section of scummvm.ini (section-scoped).
# Returns the previous value. Used to disable the Roger overlay during RECORD
# (the event recorder's on-screen control panel + screenshot grab fight the
# full-screen overlay and black out the game). The recording is input-only, so
# recording with the overlay OFF still produces a recording that reproduces the
# bug on playback with the overlay ON.
function Set-RogerGenMode([string]$value) {
    $ini = "$env:APPDATA\ScummVM\scummvm.ini"
    $txt = Get-Content $ini
    $out = New-Object System.Collections.Generic.List[string]
    $inQfg1 = $false; $prev = ""; $replaced = $false
    foreach ($line in $txt) {
        if ($line -match '^\s*\[(.+)\]\s*$') { $inQfg1 = ($matches[1] -eq 'qfg1') }
        if ($inQfg1 -and $line -match '^\s*roger_gen_mode\s*=\s*(.+)\s*$') {
            $prev = $matches[1].Trim()
            $out.Add("roger_gen_mode=$value"); $replaced = $true; continue
        }
        $out.Add($line)
    }
    if (-not $replaced) { Write-Warning "roger_gen_mode not found under [qfg1]; not changed." }
    Set-Content -Path $ini -Value $out -Encoding ASCII
    return $prev
}

if ($Record) {
    Write-Host "=== RECORD MODE (the only manual run) ===" -ForegroundColor Cyan
    Write-Host "The Roger overlay is temporarily OFF for this run (so the recorder doesn't" -ForegroundColor DarkGray
    Write-Host "black out the screen). You'll see the plain native game - that's expected." -ForegroundColor DarkGray
    Write-Host "Play the intro -> character creation -> TOWN, then QUIT (Ctrl+Q)." -ForegroundColor Cyan
    $prev = Set-RogerGenMode "prebuilt"
    Write-Host "  (roger_gen_mode: $prev -> prebuilt for recording)" -ForegroundColor DarkGray
    try {
        Remove-Item $Log -ErrorAction SilentlyContinue
        & $Exe "--record-mode=record" "--record-file-name=$Name" $Game
    } finally {
        $restore = if ($prev) { $prev } else { "cache" }
        Set-RogerGenMode $restore | Out-Null
        Write-Host "  (roger_gen_mode restored -> $restore)" -ForegroundColor DarkGray
    }
    Write-Host ""
    Write-Host "Recording done. Replay + validate (overlay ON) with:  .\roger_spike.ps1" -ForegroundColor Green
    return
}

# --- Playback + validate (no user input) -------------------------------------
Write-Host "=== PLAYBACK + VALIDATE (self-driving) ===" -ForegroundColor Cyan
Remove-Item $Log -ErrorAction SilentlyContinue

$pbArgs = @("--record-mode=fast_playback", "--record-file-name=$Name", $Game)
if ($Headless) {
    $env:SDL_VIDEODRIVER = "dummy"
    $env:SDL_AUDIODRIVER = "dummy"
    $pbArgs += "--disable-display"
    Write-Host "  (headless: dummy SDL drivers)" -ForegroundColor DarkGray
}

& $Exe @pbArgs
$exitCode = $LASTEXITCODE
if ($Headless) { Remove-Item Env:\SDL_VIDEODRIVER, Env:\SDL_AUDIODRIVER -ErrorAction SilentlyContinue }

if (-not (Test-Path $Log)) { Write-Error "No scummvm.log produced - playback may have failed (exit $exitCode)." }
$saved = "$Out\spike-$Name.log"
Copy-Item $Log $saved -Force
$lines = Get-Content $saved

# --- Validation signature -----------------------------------------------------
$reached   = ($lines | Select-String -Pattern "enter room $ExpectPic\b").Count -gt 0
$addToPic  = ($lines | Select-String -Pattern "ROGER-DIAG\[addToPic\]: pic=$ExpectPic\b").Count
$genStamps = ($lines | Select-String -Pattern "ROGER-DIAG\[genRegions\]: pic=$ExpectPic\b").Count
$missing   = ($lines | Select-String -Pattern "missing hires cel").Count

Write-Host ""
Write-Host "-------- SPIKE RESULT (pic $ExpectPic) --------" -ForegroundColor White
Write-Host ("  reached pic {0}        : {1}" -f $ExpectPic, $(if ($reached) {"YES"} else {"NO"}))
Write-Host ("  addToPic statics      : {0}  (0 == the bug: missing views)" -f $addToPic)
Write-Host ("  Feeder B native stamps: {0}  (>0 == blocky native bleed-through)" -f $genStamps)
Write-Host ("  'missing hires cel'   : {0}" -f $missing)
Write-Host ("  log                   : {0}" -f $saved)
Write-Host "-----------------------------------------------" -ForegroundColor White

if (-not $reached) {
    Write-Host "INCONCLUSIVE: playback never reached pic $ExpectPic. Re-record? (.\roger_spike.ps1 -Record)" -ForegroundColor Yellow
    exit 2
}
if ($addToPic -eq 0) {
    Write-Host "ISSUE REPRODUCED: pic $ExpectPic entered with 0 captured statics -> views missing in overlay." -ForegroundColor Red
    exit 1
}
Write-Host "HEALTHY: pic $ExpectPic captured $addToPic statics -> views present." -ForegroundColor Green
exit 0
