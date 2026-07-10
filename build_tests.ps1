# build_tests.ps1 — Generate, build, and run the ScummVM CxxTest test runner (MSVC)
#
# This is the TDD verification harness. It produces a SEPARATE tests solution in
# dists\msvc-tests\ (the game-build solution in dists\msvc\ is left untouched),
# builds the CxxTest runner with MSBuild (Release|x64), runs it, and surfaces the
# pass/fail output and exit code.
#
# Per-task incremental cycle (fast, ~10-60s once the first full build is cached):
#     .\build_tests.ps1
#
# Force a clean regeneration of the tests solution (slow, full rebuild):
#     .\build_tests.ps1 -Regenerate
#
# PREREQUISITES: Visual Studio 2019/2022 (C++ workload), vcpkg deps already
# installed at C:\vcpkg\installed\x64-windows, create_project.exe already built,
# and python3 on PATH (CxxTest's cxxtestgen is a Python script).

param(
    [string]$Config   = "Release",
    [string]$Platform = "x64",
    [switch]$Regenerate
)

$ErrorActionPreference = "Stop"

$Root      = $PSScriptRoot
$DistsDir  = "$Root\dists\msvc-tests"
$Solution  = "$DistsDir\scummvm-tests.sln"

# Absolute path to the Roger test fixtures, baked into the build as FIXTURE_DIR.
# Forward slashes keep the C++ string literal free of escape headaches; both
# Common::String concatenation and Common::Path accept '/' on Windows.
$FixtureDir = ("$Root\test\sci\roger\fixtures" -replace '\\','/')

# The Roger production sources the unit tests link against. create_project's --tests
# path force-disables every engine and its module.mk parser treats
# `ifeq ($(ENABLE_SCI), STATIC_PLUGIN)` as always-false, so neither the Roger test
# suites nor any Roger source is wired in automatically; we inject them here.
#
# IMPORTANT: only SCI-TYPE-FREE Roger units belong here. The plan deliberately keeps
# the loaders/compositor (png_loader, roger_coords, view_cache, slice_set,
# roger_compositor) free of SCI engine types so they are unit-testable. The SCI-glue
# files (file_roger_art_provider.cpp, roger_art_provider.cpp) are NOT compiled into
# the test exe: they #include sci/graphics/screen.h (and, from Task 7, animate.h),
# which drag in the full SCI graphics/engine stack and cannot link into this minimal
# CxxTest runner. Those files are verified by the full scummvm.sln build + game-run
# instead. Add new SCI-free .cpp here as later tasks create them (view_cache.cpp,
# slice_set.cpp, roger_compositor.cpp). roger_coords.h is header-only (no entry).
#
# Paths are relative to the solution dir (dists\msvc-tests), matching the ..\..\
# form create_project emits for every other ClCompile entry. Absolute paths break
# MSBuild's per-file intermediate-dir computation.
$RogerSources = @(
    "..\..\engines\sci\roger\gen\roger_pic_parser.cpp",
    "..\..\engines\sci\roger\gen\roger_pic_native.cpp",
    "..\..\engines\sci\roger\gen\roger_ega_blend.cpp",
    "..\..\engines\sci\roger\gen\roger_omyac.cpp",
    "..\..\engines\sci\roger\gen\roger_scale.cpp",
    "..\..\engines\sci\roger\gen\roger_asset_gen.cpp",
    "..\..\engines\sci\roger\png_loader.cpp",
    "..\..\engines\sci\roger\overlay\view_cache.cpp",
    "..\..\engines\sci\roger\gen\slice_set.cpp",
    "..\..\engines\sci\roger\overlay\roger_effects.cpp",
    "..\..\engines\sci\roger\overlay\roger_cursor.cpp",
    "..\..\engines\sci\roger\overlay\roger_compositor.cpp",
    "..\..\engines\sci\roger\overlay\roger_text.cpp",
    "..\..\engines\sci\roger\overlay\roger_palette_remap.cpp",
    "..\..\engines\sci\roger\roger_selftest.cpp",
    "..\..\engines\sci\roger\ui\roger_widgets.cpp",
    "..\..\engines\sci\roger\ui\roger_panel_style.cpp",
    "..\..\engines\sci\roger\utils\studio\roger_studio_render.cpp",
    "..\..\engines\sci\roger\roger_input.cpp",
    "..\..\engines\sci\roger\gen\roger_view_scaler.cpp",
    "..\..\engines\sci\roger\overlay\roger_journal.cpp",
    "..\..\engines\sci\roger\utils\tunepanel\roger_tune_panel.cpp",
    "..\..\engines\sci\roger\utils\eyetest\roger_eyetest_search.cpp",
    "..\..\engines\sci\roger\gen\roger_passes.cpp",
    "..\..\engines\sci\roger\launcher\roger_picker_model.cpp"
)

# The Roger test-suite headers to feed cxxtestgen, listed explicitly rather than via
# a *.h glob so we can EXCLUDE test_file_roger_art_provider.h (it links the SCI-glue
# provider, which is not in the test exe — see above). Add a header here as each task
# creates it (test_load_surface.h, test_roger_coords.h, test_view_cache.h,
# test_slice_set.h, test_compositor.h).
$RogerTestHeaders = @(
    "test_byte_reader.h",
    "test_pic_parser.h",
    "test_omyac.h",
    "test_ega_blend.h",
    "test_scale.h",
    "test_asset_cache.h",
    "test_png_loader.h",
    "test_load_surface.h",
    "test_roger_coords.h",
    "test_view_cache.h",
    "test_slice_set.h",
    "test_compositor.h",
    "test_cursor.h",
    "test_ui_layer.h",
    "test_roger_text.h",
    "test_ui_render.h",
    "test_palette_remap.h",
    "test_roger_selftest.h",
    "test_omyac_params.h",
    "test_studio_render.h",
    "test_shift_lock.h",
    "test_generic_text_capture.h",
    "test_input_script.h",
    "test_view_scaler.h",
    "test_roger_journal.h",
    "test_tune_panel.h",
    "test_eyetest_search.h",
    "test_roger_passes.h",
    "test_present_barrier.h",
    "test_foreground_capture.h",
    "test_window_stamp_filter.h",
    "test_charscreen_fidelity.h",
    "test_compare_mode.h",
    "test_effects.h",
    "test_roger_capabilities.h",
    "test_picker_model.h"
)

# ── Locate MSBuild via vswhere ────────────────────────────────────────────────
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "Visual Studio not found. Install VS 2019/2022 with C++ workload."
}
$MSBuild = (& $vswhere -latest -requires Microsoft.Component.MSBuild `
    -find "MSBuild\**\Bin\MSBuild.exe") | Select-Object -First 1
if (-not $MSBuild) { Write-Error "MSBuild not found." }
Write-Host "MSBuild: $MSBuild" -ForegroundColor DarkGray

$VSRoot = (& $vswhere -latest -property installationPath)

# ── Load MSVC environment (for cl.exe, and so python3/cxxtestgen run cleanly) ──
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

# ── Point the generated .props at the vcpkg installed tree + lib aliases ──────
# Identical machinery to build_and_run.ps1: the test exe links the same
# common/graphics/image/audio libs and therefore the same third-party libs.
$env:SCUMMVM_LIBS = "C:\vcpkg\installed\x64-windows"
Write-Host "SCUMMVM_LIBS: $env:SCUMMVM_LIBS" -ForegroundColor DarkGray
if (-not (Test-Path "$env:SCUMMVM_LIBS\lib")) {
    Write-Error "vcpkg libs not found at $env:SCUMMVM_LIBS\lib. Run build_and_run.ps1 once to install deps."
}
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

# ── create_project.exe ─────────────────────────────────────────────────────────
$CpExe = "$Root\devtools\create_project\msvc\Release\create_project.exe"
if (-not (Test-Path $CpExe)) {
    Write-Error "create_project.exe not found at $CpExe. Build it first (see build_and_run.ps1 step 2)."
}

# ── Post-processor: wire Roger into the generated tests project ───────────────
# create_project cannot do this itself (see notes on $RogerSources). We patch the
# generated files in place. This is idempotent and per-item, so it runs on every
# build (not just on generation): when a later task adds a .cpp to $RogerSources or
# a header to $RogerTestHeaders, re-running the script wires in only the new items
# and triggers an incremental rebuild — no full -Regenerate needed.
function Sync-TestsProject {
    $vcxproj = "$DistsDir\scummvm-tests.vcxproj"
    if (-not (Test-Path $vcxproj)) { Write-Error "Generated $vcxproj missing." }

    $proj = Get-Content $vcxproj -Raw
    $projOrig = $proj
    $anchor = '<ClCompile Include="test/runner/test_runner.cpp" />'
    if ($proj -notmatch [regex]::Escape($anchor)) {
        Write-Error "Could not find test runner ClCompile anchor in $vcxproj."
    }

    # 1) Add each Roger .cpp next to the test runner ClCompile, only if missing
    #    (per-source idempotency lets later tasks append to $RogerSources).
    foreach ($src in $RogerSources) {
        $entry = "<ClCompile Include=`"$src`" />"
        if (-not $proj.Contains($entry)) {
            $proj = $proj.Replace($anchor, "$anchor`r`n`t`t$entry")
        }
    }

    # 1b) Exclude backends\saves\savefile.cpp from the build. test/system/null_osystem.cpp
    #     #includes that file inline (so its OutSaveFile/SaveFileManager definitions land in
    #     null_osystem.obj); compiling it again as its own translation unit produces a second
    #     savefile.obj with the same symbols -> LNK2005. The make-based test build never
    #     compiles backends/saves/savefile.o separately for exactly this reason.
    $dupSrc = '<ClCompile Include="..\..\backends\saves\savefile.cpp" />'
    if ($proj.Contains($dupSrc)) {
        $proj = $proj.Replace($dupSrc,
            '<ClCompile Include="..\..\backends\saves\savefile.cpp"><ExcludedFromBuild>true</ExcludedFromBuild></ClCompile>')
    }

    # 2) Make the runner ROGER-ONLY: replace cxxtestgen's entire input-file list
    #    (everything after the -o output path, up to the closing </Command>) with just
    #    the Roger test headers. We are doing Roger TDD; the default common/image/audio/
    #    math/graphics suites add only noise here — they include pre-existing failures
    #    on this MSVC config (iconv-based CJK encoding) and FS tests, and one of them
    #    aborts the whole runner. Listing the Roger headers explicitly (not a *.h glob)
    #    also excludes test_file_roger_art_provider.h, which links the SCI-glue provider
    #    that is intentionally not in the test exe.
    #    The ../../ hop from the solution dir (dists\msvc-tests) to the repo root is
    #    fixed, exactly as build_and_run.ps1 relies on. A MatchEvaluator builds the
    #    replacement so the literal $(SolutionDir) is not mangled by -replace's $ rules.
    $rogerBase = '$(SolutionDir)../../test/sci/roger/'
    $rogerInputs = ($RogerTestHeaders | ForEach-Object { "$rogerBase$_" }) -join " "
    $cmdPattern = '(-o &quot;\$\(SolutionDir\)test/runner/test_runner\.cpp&quot; )[^<]*'
    $proj = [regex]::Replace($proj, $cmdPattern, { param($mm) $mm.Groups[1].Value + $rogerInputs })

    # Only rewrite if something changed, so unchanged re-runs don't bump the mtime
    # and force MSBuild to recompile the world.
    if ($proj -ne $projOrig) {
        Set-Content $vcxproj $proj -Encoding UTF8 -NoNewline
    }

    # 3) Add FIXTURE_DIR to the x64 global props PreprocessorDefinitions.
    #    Release|x64 imports Releasex64.props which imports Globalx64.props, where the
    #    shared <PreprocessorDefinitions> live. FIXTURE_DIR must expand to a quoted
    #    C++ string literal, so the embedded quotes are XML-escaped (&quot;).
    #    NOTE: we deliberately do NOT define ENABLE_SCI. We compile only the SCI-free
    #    Roger units, not the whole SCI engine; defining ENABLE_SCI would make
    #    base/plugins.cpp emit a LINK_PLUGIN(SCI) reference to g_SCI_type/g_SCI_getObject
    #    -> LNK2001 unresolved externals, and would also change how the SCI graphics
    #    headers compile (FontSJIS), which we avoid by not pulling them in at all.
    $globalProps = "$DistsDir\ScummVMTests_Global$Platform.props"
    if (-not (Test-Path $globalProps)) { Write-Error "Global props $globalProps missing." }
    $props = Get-Content $globalProps -Raw
    $define = "FIXTURE_DIR=&quot;$FixtureDir&quot;;"
    if ($props -notmatch 'FIXTURE_DIR') {
        $props = $props -replace '(<PreprocessorDefinitions>)', "`$1$define"
        Set-Content $globalProps $props -Encoding UTF8 -NoNewline
    }

    Write-Host "  Synced tests project: Roger sources [$($RogerSources.Count)], test headers [$($RogerTestHeaders.Count)], savefile excluded, FIXTURE_DIR set." -ForegroundColor DarkGray
}

# ── Step 1: Generate the tests solution if needed (idempotent) ────────────────
if ($Regenerate -and (Test-Path $DistsDir)) {
    Write-Host "Regenerate requested - removing $DistsDir" -ForegroundColor Yellow
    Remove-Item $DistsDir -Recurse -Force
}

if (-not (Test-Path $Solution)) {
    Write-Host "`n[1/3] Generating scummvm-tests.sln into dists\msvc-tests ..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Force -Path $DistsDir | Out-Null
    Push-Location $DistsDir
    # Mirror build_and_run.ps1 ordering: --disable-all-engines BEFORE --enable-engine=sci.
    # (Stock create_project re-disables all engines under --tests anyway; the SCI
    #  wiring is completed by Edit-TestsProject below.)
    & $CpExe ..\.. --msvc --tests --disable-all-engines --enable-engine=sci
    $result = $LASTEXITCODE
    Pop-Location
    if ($result -ne 0) { Write-Error "Tests project generation failed." }
    Write-Host "  scummvm-tests.sln generated." -ForegroundColor Green
} else {
    Write-Host "[1/3] scummvm-tests.sln already exists (use -Regenerate to rebuild from scratch)." -ForegroundColor DarkGray
}

# Sync Roger sources/test-headers into the project on EVERY run (idempotent). This
# is what lets a later task add a .cpp/.h and get an incremental rebuild without a
# full -Regenerate.
Sync-TestsProject

# ── Step 2: Build the tests solution ──────────────────────────────────────────
Write-Host "`n[2/3] Building test runner ($Config|$Platform)..." -ForegroundColor Cyan
Write-Host "      First build compiles common/graphics/image/audio + SCI/Roger; expect 10-20 min." -ForegroundColor DarkGray
# The generated project's PostBuildEvent runs the test exe with IgnoreExitCode, so
# MSBuild succeeds even if a test fails. We re-run the exe ourselves below to get a
# real exit code, so suppress that auto-run here is not possible — but it's harmless.
& $MSBuild $Solution /p:Configuration=$Config /p:Platform=$Platform /m /nologo /v:minimal
if ($LASTEXITCODE -ne 0) { Write-Error "Test runner build failed." }

# ── Find the test runner exe ──────────────────────────────────────────────────
$candidates = @(
    "$DistsDir\$Config$Platform\scummvm-tests.exe",
    "$DistsDir\$Config\$Platform\scummvm-tests.exe",
    "$DistsDir\$Platform\$Config\scummvm-tests.exe",
    "$DistsDir\$Config\scummvm-tests.exe"
)
$Exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $Exe) {
    $Exe = Get-ChildItem -Path $DistsDir -Recurse -Filter scummvm-tests.exe -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $Exe) {
    Write-Error "scummvm-tests.exe not found. Searched:`n$($candidates -join "`n")"
}

# ── Step 3: Run the test runner and surface pass/fail + exit code ─────────────
Write-Host "`n[3/3] Running test runner: $Exe" -ForegroundColor Cyan
Write-Host ("-" * 70)
& $Exe
$testExit = $LASTEXITCODE
Write-Host ("-" * 70)
if ($testExit -eq 0) {
    Write-Host "TESTS PASSED (exit $testExit)" -ForegroundColor Green
} else {
    Write-Host "TESTS FAILED (exit $testExit)" -ForegroundColor Red
}
exit $testExit
