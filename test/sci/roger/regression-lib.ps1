# Shared helpers for the Roger regression gate (dot-source me).
# PS 5.1. Pixel work uses LockBits: GetPixel at 2611x1743 costs minutes; this costs ms.
Add-Type -AssemblyName System.Drawing

# Returns @{BufA;BufB;Stride;Width;Height} of 32bppArgb rows for the fractional
# region, or $null on image size mismatch. Internal helper.
function Get-RegionBuffers {
    param([string]$PathA, [string]$PathB,
          [double]$X0, [double]$Y0, [double]$X1, [double]$Y1)
    $a = [System.Drawing.Bitmap]::FromFile($PathA)
    $b = [System.Drawing.Bitmap]::FromFile($PathB)
    try {
        if ($a.Width -ne $b.Width -or $a.Height -ne $b.Height) { return $null }
        $rx = [int]($a.Width * $X0); $ry = [int]($a.Height * $Y0)
        $rw = [int]($a.Width * ($X1 - $X0)); $rh = [int]($a.Height * ($Y1 - $Y0))
        if ($rw -le 0 -or $rh -le 0) { throw "empty region" }
        $rect = [System.Drawing.Rectangle]::new($rx, $ry, $rw, $rh)
        $fmt = [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
        $da = $a.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, $fmt)
        $db = $b.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, $fmt)
        $bytes = $da.Stride * $rh
        $bufA = New-Object byte[] $bytes
        $bufB = New-Object byte[] $bytes
        [System.Runtime.InteropServices.Marshal]::Copy($da.Scan0, $bufA, 0, $bytes)
        [System.Runtime.InteropServices.Marshal]::Copy($db.Scan0, $bufB, 0, $bytes)
        $a.UnlockBits($da); $b.UnlockBits($db)
        return @{ BufA = $bufA; BufB = $bufB; Stride = $da.Stride; Width = $rw; Height = $rh }
    } finally { $a.Dispose(); $b.Dispose() }
}

# RGB equality over the region (alpha ignored). Fast path: whole-row memcmp via
# SequenceEqual — .NET-side, so the common all-equal case never enters a PS loop.
function Test-RegionEqual {
    param([string]$PathA, [string]$PathB,
          [double]$X0, [double]$Y0, [double]$X1, [double]$Y1)
    $r = Get-RegionBuffers -PathA $PathA -PathB $PathB -X0 $X0 -Y0 $Y0 -X1 $X1 -Y1 $Y1
    if ($null -eq $r) { return $false }
    $rowLen = $r.Width * 4
    for ($row = 0; $row -lt $r.Height; $row++) {
        $off = $row * $r.Stride
        $ra = New-Object byte[] $rowLen
        $rb = New-Object byte[] $rowLen
        [System.Array]::Copy($r.BufA, $off, $ra, 0, $rowLen)
        [System.Array]::Copy($r.BufB, $off, $rb, 0, $rowLen)
        # 32bppArgb layout is BGRA: zero both alpha bytes so only RGB compares.
        for ($i = 3; $i -lt $rowLen; $i += 4) { $ra[$i] = 0; $rb[$i] = 0 }
        if (-not [System.Linq.Enumerable]::SequenceEqual($ra, $rb)) { return $false }
    }
    return $true
}

# Count of RGB-differing pixels in the region. -StopAt N early-exits once N is
# reached (use for presence checks; the full count is only for diagnostics).
function Get-RegionDiffCount {
    param([string]$PathA, [string]$PathB,
          [double]$X0, [double]$Y0, [double]$X1, [double]$Y1,
          [int]$StopAt = 0)
    $r = Get-RegionBuffers -PathA $PathA -PathB $PathB -X0 $X0 -Y0 $Y0 -X1 $X1 -Y1 $Y1
    if ($null -eq $r) { return -1 }
    $diff = 0
    for ($row = 0; $row -lt $r.Height; $row++) {
        $off = $row * $r.Stride
        for ($px = 0; $px -lt $r.Width; $px++) {
            $i = $off + $px * 4
            if ($r.BufA[$i] -ne $r.BufB[$i] -or
                $r.BufA[$i + 1] -ne $r.BufB[$i + 1] -or
                $r.BufA[$i + 2] -ne $r.BufB[$i + 2]) {
                $diff++
                if ($StopAt -gt 0 -and $diff -ge $StopAt) { return $diff }
            }
        }
    }
    return $diff
}

# Resolve a capture label to its single preview PNG (pic id varies by room).
function Find-Capture {
    param([string]$ShotsDir, [string]$Label)
    $m = @(Get-ChildItem -Path (Join-Path $ShotsDir "roger-*-$Label-preview.png") -ErrorAction SilentlyContinue)
    if ($m.Count -ne 1) { throw "expected exactly 1 capture for label '$Label', found $($m.Count)" }
    return $m[0].FullName
}

# Parse ROGER-CYCLE telemetry (animate.cpp: "ROGER-CYCLE period=%u busy=%u").
function Get-CycleStats {
    param([string]$LogPath, [int]$SkipFirst = 10)
    $periods = New-Object System.Collections.Generic.List[double]
    $busys   = New-Object System.Collections.Generic.List[double]
    foreach ($hit in (Select-String -Path $LogPath -Pattern 'ROGER-CYCLE period=(\d+) busy=(\d+)')) {
        $periods.Add([double]$hit.Matches[0].Groups[1].Value)
        $busys.Add([double]$hit.Matches[0].Groups[2].Value)
    }
    if ($periods.Count -le $SkipFirst) { throw "only $($periods.Count) ROGER-CYCLE samples in $LogPath" }
    $p = $periods.GetRange($SkipFirst, $periods.Count - $SkipFirst)
    $bu = $busys.GetRange($SkipFirst, $busys.Count - $SkipFirst)
    if ($p.Count -lt 30) { throw "only $($p.Count) usable ROGER-CYCLE samples (need >= 30)" }
    $ps = @($p | Sort-Object)
    $bs = @($bu | Sort-Object)
    return [pscustomobject]@{
        Samples      = $p.Count
        PeriodMedian = $ps[[int][math]::Floor($ps.Count / 2)]
        PeriodP90    = $ps[[int][math]::Floor($ps.Count * 0.9)]
        BusyMedian   = $bs[[int][math]::Floor($bs.Count / 2)]
    }
}
