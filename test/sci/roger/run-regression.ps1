# Roger regression gate (spec: docs/superpowers/specs/2026-07-02-roger-present-barrier-design.md §8).
# Runs every manifest entry through build_and_run.ps1 -NoBuild and evaluates checks.
#   -Record : run perf entries and (re)write baselines/perf-baseline.json instead of checking.
# Exit 0 iff all checks PASS. Run from anywhere; paths are repo-relative to this file.
param([switch]$Record)
$ErrorActionPreference = "Stop"
. "$PSScriptRoot\regression-lib.ps1"

$Root  = (Resolve-Path "$PSScriptRoot\..\..\..").Path   # test/sci/roger -> repo root
$Shots = Join-Path $Root "screenshots"
$RunLog = Join-Path $Shots "roger-run.log"
$Harness = Join-Path $Root "build_and_run.ps1"
$BaselinePath = Join-Path $PSScriptRoot "baselines\perf-baseline.json"
$manifest = Get-Content (Join-Path $PSScriptRoot "regression-manifest.json") -Raw | ConvertFrom-Json

# ── Preflight ─────────────────────────────────────────────────────────────────
$exe = Join-Path $Root "dists\msvc\Releasex64\scummvm.exe"
if (-not (Test-Path $exe)) { Write-Error "no built exe at $exe (build first; driver always runs -NoBuild)" }
foreach ($e in $manifest.entries) {
    $save = Join-Path "$env:APPDATA\ScummVM\Saved games" ("{0}.{1:D3}" -f $e.target, $e.saveSlot)
    if (-not (Test-Path $save)) { Write-Error "missing save '$save' required by entry '$($e.name)'" }
    $scriptPath = Join-Path "$PSScriptRoot\scripts" $e.script
    if (-not (Test-Path $scriptPath)) { Write-Error "missing script '$scriptPath' (entry '$($e.name)')" }
}

$results = New-Object System.Collections.Generic.List[object]
function Add-Result($entry, $check, $ok, $detail) {
    $results.Add([pscustomobject]@{ Entry = $entry; Check = $check; Result = $(if ($ok) { "PASS" } else { "FAIL" }); Detail = $detail })
}

$perfRecord = @{}
foreach ($e in $manifest.entries) {
    $isPerf = @($e.checks | Where-Object { $_.type -eq "perf" }).Count -gt 0
    if ($Record -and -not $isPerf) { continue }   # -Record touches only perf entries

    Write-Host "=== $($e.name) ===" -ForegroundColor Cyan
    # NOTE: use hashtable splatting for named parameters (array splatting is positional only).
    $harnessArgs = @{
        Game       = $e.target
        SaveSlot   = $e.saveSlot
        Script     = (Join-Path "$PSScriptRoot\scripts" $e.script)
        TimeoutSec = $e.timeoutSec
        NoBuild    = $true
    }
    if ($e.cycleLog) { $harnessArgs["CycleLog"] = $true }
    & $Harness @harnessArgs | Out-Host
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        Add-Result $e.name "run" $false "exit code $code (124 = hung script)"
        continue
    }
    Add-Result $e.name "run" $true "exit 0"

    foreach ($c in $e.checks) {
        switch ($c.type) {
            "capturesExist" {
                foreach ($label in $c.labels) {
                    try { Find-Capture -ShotsDir $Shots -Label $label | Out-Null
                          Add-Result $e.name "exists:$label" $true "" }
                    catch { Add-Result $e.name "exists:$label" $false $_.Exception.Message }
                }
            }
            "sameRunDiff" {
                try {
                    $pa = Find-Capture -ShotsDir $Shots -Label $c.a
                    $pb = Find-Capture -ShotsDir $Shots -Label $c.b
                    $r = $c.region
                    $eq = Test-RegionEqual -PathA $pa -PathB $pb -X0 $r.x0 -Y0 $r.y0 -X1 $r.x1 -Y1 $r.y1
                    if ($eq) { Add-Result $e.name "same:$($c.a)~$($c.b)" $true "" }
                    else {
                        $n = Get-RegionDiffCount -PathA $pa -PathB $pb -X0 $r.x0 -Y0 $r.y0 -X1 $r.x1 -Y1 $r.y1
                        Add-Result $e.name "same:$($c.a)~$($c.b)" $false "$n differing px in region"
                    }
                } catch { Add-Result $e.name "same:$($c.a)~$($c.b)" $false $_.Exception.Message }
            }
            "presenceDiff" {
                try {
                    $pa = Find-Capture -ShotsDir $Shots -Label $c.a
                    $pb = Find-Capture -ShotsDir $Shots -Label $c.b
                    $r = $c.region
                    $n = Get-RegionDiffCount -PathA $pa -PathB $pb -X0 $r.x0 -Y0 $r.y0 -X1 $r.x1 -Y1 $r.y1 -StopAt $c.minDiffPx
                    if ($n -ge $c.minDiffPx) { Add-Result $e.name "presence:$($c.b)" $true "$n px (>= $($c.minDiffPx))" }
                    else { Add-Result $e.name "presence:$($c.b)" $false "only $n differing px (< $($c.minDiffPx))" }
                } catch { Add-Result $e.name "presence:$($c.b)" $false $_.Exception.Message }
            }
            "perf" {
                try {
                    $s = Get-CycleStats -LogPath $RunLog
                    if ($Record) {
                        $perfRecord[$e.name] = @{ periodMedian = $s.PeriodMedian; periodP90 = $s.PeriodP90; busyMedian = $s.BusyMedian }
                        Add-Result $e.name "perf" $true "recorded median=$($s.PeriodMedian) p90=$($s.PeriodP90) busy=$($s.BusyMedian) (n=$($s.Samples))"
                    } else {
                        if (-not (Test-Path $BaselinePath)) { throw "no perf baseline; run with -Record first" }
                        $base = (Get-Content $BaselinePath -Raw | ConvertFrom-Json).$($e.name)
                        if ($null -eq $base) { throw "no baseline key '$($e.name)'; re-run -Record" }
                        $ok = ($s.PeriodMedian -le $base.periodMedian * 1.05) -and
                              ($s.BusyMedian   -le $base.busyMedian + 1) -and
                              ($s.PeriodP90    -le $base.periodP90 * 1.10)
                        Add-Result $e.name "perf" $ok ("median=$($s.PeriodMedian)/$($base.periodMedian) p90=$($s.PeriodP90)/$($base.periodP90) busy=$($s.BusyMedian)/$($base.busyMedian) n=$($s.Samples)")
                    }
                } catch { Add-Result $e.name "perf" $false $_.Exception.Message }
            }
            default { Add-Result $e.name $c.type $false "unknown check type" }
        }
    }
}

if ($Record -and $perfRecord.Count -gt 0) {
    New-Item -ItemType Directory -Force (Split-Path $BaselinePath) | Out-Null
    $perfRecord | ConvertTo-Json -Depth 4 | Out-File $BaselinePath -Encoding utf8
    Write-Host "Baselines written to $BaselinePath" -ForegroundColor Yellow
}

$results | Format-Table -AutoSize | Out-Host
$failCount = @($results | Where-Object { $_.Result -eq "FAIL" }).Count
if ($failCount -gt 0) { Write-Host "$failCount FAILURES" -ForegroundColor Red; exit 1 }
Write-Host "ALL PASS ($($results.Count) checks)" -ForegroundColor Green
exit 0
