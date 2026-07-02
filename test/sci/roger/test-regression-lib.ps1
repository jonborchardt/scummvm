# Self-test for regression-lib.ps1. Run: powershell -File test-regression-lib.ps1
# Exits 0 on all-pass, 1 on any failure.
$ErrorActionPreference = "Stop"
. "$PSScriptRoot\regression-lib.ps1"
Add-Type -AssemblyName System.Drawing

$tmp = Join-Path $env:TEMP "roger-lib-selftest"
New-Item -ItemType Directory -Force $tmp | Out-Null
$fails = 0
function Check($name, $cond) {
    if ($cond) { Write-Host "PASS $name" -ForegroundColor Green }
    else { Write-Host "FAIL $name" -ForegroundColor Red; $script:fails++ }
}

# Fixture: two 200x100 bitmaps, B differs from A in a 10x10 block at (20,80)
# (inside the bottom-quarter region) and in 1 px at (5,5) (outside it).
$a = New-Object System.Drawing.Bitmap 200, 100
$b = New-Object System.Drawing.Bitmap 200, 100
$g = [System.Drawing.Graphics]::FromImage($a); $g.Clear([System.Drawing.Color]::Navy); $g.Dispose()
$g = [System.Drawing.Graphics]::FromImage($b); $g.Clear([System.Drawing.Color]::Navy); $g.Dispose()
for ($y = 80; $y -lt 90; $y++) { for ($x = 20; $x -lt 30; $x++) { $b.SetPixel($x, $y, [System.Drawing.Color]::Red) } }
$b.SetPixel(5, 5, [System.Drawing.Color]::Red)
$pa = Join-Path $tmp "a.png"; $pb = Join-Path $tmp "b.png"
$a.Save($pa); $b.Save($pb); $a.Dispose(); $b.Dispose()

Check "identical file equal (full)"      (Test-RegionEqual -PathA $pa -PathB $pa -X0 0 -Y0 0 -X1 1 -Y1 1)
Check "differing files unequal (full)"   (-not (Test-RegionEqual -PathA $pa -PathB $pb -X0 0 -Y0 0 -X1 1 -Y1 1))
Check "clean sub-region equal"           (Test-RegionEqual -PathA $pa -PathB $pb -X0 0.5 -Y0 0 -X1 1 -Y1 0.5)
Check "diff count = 100 in bottom band"  ((Get-RegionDiffCount -PathA $pa -PathB $pb -X0 0 -Y0 0.75 -X1 1 -Y1 1) -eq 100)
Check "StopAt early-exit caps count"     ((Get-RegionDiffCount -PathA $pa -PathB $pb -X0 0 -Y0 0.75 -X1 1 -Y1 1 -StopAt 7) -ge 7)

# Find-Capture: fake shots dir
$shots = Join-Path $tmp "shots"
New-Item -ItemType Directory -Force $shots | Out-Null
Copy-Item $pa (Join-Path $shots "roger-320-mylabel-preview.png")
Copy-Item $pa (Join-Path $shots "roger-320-other-preview.png")
Check "Find-Capture finds single match"  ((Find-Capture -ShotsDir $shots -Label "mylabel") -like "*roger-320-mylabel-preview.png")
$threw = $false
try { Find-Capture -ShotsDir $shots -Label "absent" | Out-Null } catch { $threw = $true }
Check "Find-Capture throws on 0 matches" $threw

# Get-CycleStats: synthetic log — 10 boot samples (skipped) + 40 steady samples
$log = Join-Path $tmp "run.log"
$lines = @()
for ($i = 0; $i -lt 10; $i++) { $lines += "WARNING: ROGER-CYCLE period=300 busy=250!" }
for ($i = 0; $i -lt 40; $i++) { $lines += "WARNING: ROGER-CYCLE period=$(80 + ($i % 5)) busy=12!" }
$lines | Out-File $log -Encoding utf8
$s = Get-CycleStats -LogPath $log
Check "stats sample count"   ($s.Samples -eq 40)
Check "period median sane"   ($s.PeriodMedian -ge 80 -and $s.PeriodMedian -le 85)
Check "busy median"          ($s.BusyMedian -eq 12)
Check "p90 >= median"        ($s.PeriodP90 -ge $s.PeriodMedian)
$threw = $false
$short = Join-Path $tmp "short.log"
"WARNING: ROGER-CYCLE period=80 busy=12!" | Out-File $short -Encoding utf8
try { Get-CycleStats -LogPath $short | Out-Null } catch { $threw = $true }
Check "throws on <30 samples" $threw

if ($fails -gt 0) { Write-Host "$fails FAILURES" -ForegroundColor Red; exit 1 }
Write-Host "ALL PASS" -ForegroundColor Green; exit 0
