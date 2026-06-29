# roger_autodrive.ps1 -- self-driving LIVE repro of the QFG1 fresh-start town.
#
# Launches QFG1 normally (overlay ON, roger_diag ON -- NO event recorder, so no
# black screen), drives from a fresh start through the intro + character creation
# into the town via focused keystrokes/mouse clicks, then closes gracefully so
# scummvm.log flushes. Self-validating: reports which rooms were reached and the
# pic-300 diag signature. Tune $Steps from the log feedback until it reaches 300.
#
#   .\roger_autodrive.ps1
#
param(
    [string]$Game = "qfg1"
)
$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot
$Exe  = "$Root\dists\msvc\Releasex64\scummvm.exe"
$Log  = "$env:APPDATA\ScummVM\Logs\scummvm.log"
$Out  = "$Root\screenshots\spike"
New-Item -ItemType Directory -Force $Out | Out-Null

$wsh = New-Object -ComObject WScript.Shell

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class U32 {
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
$MOUSEEVENTF_LEFTDOWN = 0x02; $MOUSEEVENTF_LEFTUP = 0x04

function Focus-Game($p) {
    for ($i=0; $i -lt 5; $i++) { if ($wsh.AppActivate($p.Id)) { return $true }; Start-Sleep -Milliseconds 200 }
    return $false
}
function Click-Center($p) {
    Focus-Game $p | Out-Null
    $r = New-Object U32+RECT
    if ([U32]::GetWindowRect($p.MainWindowHandle, [ref]$r)) {
        $cx = [int](($r.Left + $r.Right)/2); $cy = [int](($r.Top + $r.Bottom)/2)
        [U32]::SetCursorPos($cx, $cy) | Out-Null
        [U32]::mouse_event($MOUSEEVENTF_LEFTDOWN,0,0,0,[IntPtr]::Zero)
        [U32]::mouse_event($MOUSEEVENTF_LEFTUP,0,0,0,[IntPtr]::Zero)
    }
}
function Send-Key($p, $k) { Focus-Game $p | Out-Null; $wsh.SendKeys($k) }

# Sequence: each step is an action + post-delay (ms). Intro story scenes advance
# on click/Enter; character creation: Tab to the Start/Done button, Enter, then
# accept the unspent-points warning. Generous delays (intro animates).
$Steps = @(
    @{a="click"; d=3000}, @{a="enter"; d=3000},   # title / scene 1
    @{a="click"; d=3000}, @{a="enter"; d=3000},   # scene 2
    @{a="click"; d=3000}, @{a="enter"; d=3000},   # scene 3
    @{a="click"; d=3000}, @{a="enter"; d=3000},   # scene 4
    @{a="click"; d=3000}, @{a="enter"; d=3000},   # scene 5 / class picker
    @{a="enter"; d=3000},                          # select first hero (Fighter)
    @{a="enter"; d=3500},                          # confirm -> char creation
    @{a="tab";   d=400},  @{a="tab"; d=400}, @{a="tab"; d=400},
    @{a="tab";   d=400},  @{a="tab"; d=400}, @{a="tab"; d=400},
    @{a="enter"; d=2500},                          # Start
    @{a="enter"; d=5000},                          # accept warning -> town loads
    @{a="enter"; d=3000}, @{a="enter"; d=3000}     # greeting dialog(s)
)

Remove-Item $Log -ErrorAction SilentlyContinue
Write-Host "Launching QFG1 (live, overlay+diag on) and auto-driving to town..." -ForegroundColor Cyan
$p = Start-Process -FilePath $Exe -ArgumentList @($Game) -PassThru
Start-Sleep -Seconds 7

foreach ($s in $Steps) {
    if ($p.HasExited) { break }
    switch ($s.a) {
        "click" { Click-Center $p }
        "enter" { Send-Key $p "{ENTER}" }
        "tab"   { Send-Key $p "{TAB}" }
    }
    Start-Sleep -Milliseconds $s.d
}

Start-Sleep -Seconds 2
# Graceful close so the log flushes (force-kill leaves null bytes).
$p.CloseMainWindow() | Out-Null; Start-Sleep -Seconds 3
if (-not $p.HasExited) { $p.CloseMainWindow() | Out-Null; Start-Sleep 2 }
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
Start-Sleep 1

# Strip null bytes (in case of a non-clean exit) and report.
$clean = (Get-Content $Log -Raw) -replace "`0",""
$clean | Out-File "$Out\autodrive.log" -Encoding utf8
$lines = $clean -split "`r?`n"
Write-Host ""
Write-Host "Rooms entered:" -ForegroundColor White
$lines | Where-Object { $_ -match "enter room \d+" } | ForEach-Object { "  $_" }
$reached = ($lines | Where-Object { $_ -match "enter room 300" }).Count
$addToPic = ($lines | Where-Object { $_ -match "addToPic\]: pic=300" }).Count
if ($reached -gt 0) {
    Write-Host "REACHED TOWN. addToPic(300)=$addToPic (0 == bug reproduced). Log: $Out\autodrive.log" -ForegroundColor Green
} else {
    Write-Host "Did NOT reach town. Tune `$Steps from the rooms above. Log: $Out\autodrive.log" -ForegroundColor Yellow
}
