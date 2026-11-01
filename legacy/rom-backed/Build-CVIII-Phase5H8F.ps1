# Phase 5H-8D: isolated camera-parity diagnostics + indexed-world build.
# No upstream NESRecomp source changes, no copyrighted game assets distributed.
[CmdletBinding()]
param(
    [string]$Repo = 'C:\NESDev\CVIII-Decomp',
    [string]$Engine = 'C:\NESDev\nesrecomp',
    [string]$Rom = 'C:\NESDev\AkumajouDensetsu\Akumajou Densetsu (Japan).nes',
    [string]$Config = 'C:\NESDev\AkumajouDensetsu\game.toml',
    [string]$Ui = 'C:\NESDev\AkumajouDensetsu\recomp-ui-cviii',
    [string]$PackDir = 'C:\NESDev\AkumajouDensetsu\build-5g2\assets',
    [string]$Build = 'C:\NESDev\AkumajouDensetsu\build-5h8f',
    [string]$PythonVersion = '3.12',
    [switch]$RegeneratePacks,
    [ValidateSet('frames','full')] [string]$Diagnostic = 'full',
    [switch]$Launch
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
function Need([string]$File) {
    if (-not (Test-Path -LiteralPath $File -PathType Leaf)) { throw "Missing required file: $File" }
}
function Check([string]$Step) {
    if ($LASTEXITCODE -ne 0) { throw "$Step failed with exit code $LASTEXITCODE" }
}
$project = Join-Path $Repo 'JP\recomp\native\nesrecomp_modern'
$tools = Join-Path $Repo 'JP\recomp\tools'
$tests = Join-Path $Repo 'JP\recomp\tests'
$manifest = Join-Path $Build 'indexed-world'
$index = Join-Path $manifest 'global\CVIII-World-5H2.idx'
$roomPack = Join-Path $PackDir 'CVIII-Rooms.dat'
$gfxPack = Join-Path $PackDir 'CVIII-Graphics.dat'
$chrPack = Join-Path $PackDir 'CVIII-CHR-Index.dat'
$assetPack = Join-Path $PackDir 'CVIII-Assets.bin'
foreach ($file in @(
    $Rom, $Config, (Join-Path $Engine 'runner\cyc\project.cmake'),
    (Join-Path $project 'cviii_native_world_5h2.c'),
    (Join-Path $project 'cviii_camera_track_5h7.c'),
    (Join-Path $project 'cviii_room_local_5h8.c'),
    (Join-Path $project 'cviii_transition_5h8d.c'),
    (Join-Path $project 'cviii_transition_5h8d.h'),
    (Join-Path $project 'cviii_mutable_tiles_5h8e.c'),
    (Join-Path $project 'cviii_entity_events_5h8f.c'),
    (Join-Path $project 'cviii_entity_events_5h8f.h'),
    (Join-Path $project 'cviii_mutable_tiles_5h8e.h'),
    (Join-Path $project 'cviii_world_parity_5h5.c'),
    (Join-Path $tools 'cviii_asset_registry_5h1.py'),
    (Join-Path $tools 'cviii_build_world_index_5h2.py'),
    (Join-Path $tools 'cviii_camera_report_5h4.py'),
    (Join-Path $Ui 'src\recomp_runtime_ui.h'),
    (Join-Path $Ui 'src\common\launcher_theme.h')
)) { Need $file }
$expected = '958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28'
$actual = (Get-FileHash -LiteralPath $Rom -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actual -ne $expected) { throw "Original Japanese ROM authentication failed: $actual" }

# Only modify the private recomp-ui-cviii checkout, never shared upstream.
$theme = Join-Path $Ui 'src\common\launcher_theme.h'
$original = [IO.File]::ReadAllText($theme)
$match = [regex]::Match($original, '(?s)static\s+inline\s+LauncherTheme\s+launcher_theme_nes\(void\)\s*\{.*?\bt\.scanlines\s*=\s*(?<v>[01])\s*;')
if (-not $match.Success) { throw 'Cannot locate NES launcher theme scanlines; incompatible recomp-ui revision' }
if ($match.Groups['v'].Value -eq '1') {
    $bak = "$theme.before-5h7.bak"
    if (-not (Test-Path $bak)) { Copy-Item $theme $bak }
    $at = $match.Groups['v'].Index
    [IO.File]::WriteAllText($theme, $original.Substring(0,$at) + '0' + $original.Substring($at+1), [Text.UTF8Encoding]::new($false))
}

if ($RegeneratePacks) {
    New-Item -ItemType Directory -Path $PackDir -Force | Out-Null
    & py "-$PythonVersion" (Join-Path $tools 'jp_asset_pack_0597.py') --build --rom $Rom --out $assetPack
    Check 'CHR asset pack'
    & py "-$PythonVersion" (Join-Path $tools 'jp_asset_runtime_0598.py') --export --assets $assetPack --out $chrPack
    Check 'Physical CHR index'
    & py "-$PythonVersion" (Join-Path $tools 'build_room_resources.py') --rom $Rom --out $roomPack
    Check 'Original room pack'
    & py "-$PythonVersion" (Join-Path $tools 'build_room_graphics.py') --rom $Rom --rooms $roomPack --out $gfxPack
    Check 'Original graphics pack'
}
foreach ($file in @($roomPack, $gfxPack, $chrPack)) { Need $file }

& py "-$PythonVersion" -m unittest discover -s $tests -p 'test_cviii_camera_report_5h4.py' -v
Check '5H-7 rejection report tests'
# Generate Phase 5H-1 source-of-truth data, then the checked native 5H-2 index.
& py "-$PythonVersion" -m unittest discover -s $tests -p 'test_cviii_asset_registry_5h1.py' -v
Check '5H-1 registry tests'
& py "-$PythonVersion" -m unittest discover -s $tests -p 'test_cviii_world_index_5h2.py' -v
Check '5H-2 index tests'
& py "-$PythonVersion" -m unittest discover -s $tests -p 'test_cviii_camera_report_5h3.py' -v
Check '5H-7 camera review tests'
& py "-$PythonVersion" (Join-Path $tools 'cviii_asset_registry_5h1.py') `
    --rom $Rom --rooms $roomPack --graphics $gfxPack --chr-index $chrPack --out $manifest
Check 'Generate JSON asset registry'
& py "-$PythonVersion" (Join-Path $tools 'cviii_build_world_index_5h2.py') `
    --root $manifest --rooms $roomPack --graphics $gfxPack `
    --parser (Join-Path $tools 'cviii_asset_registry_5h1.py') --out $index
Check 'Generate native world index'
Need $index

# Keep launcher, clean NES theme, all original controls and the working 5G2 build.
& cmake --fresh -S $project -B $Build -G 'Visual Studio 18 2026' -A x64 `
    "-DNESRECOMP_EXTERNAL_ROOT=$($Engine.Replace('\','/'))" `
    "-DJP_ORIGINAL_JP_ROM=$($Rom.Replace('\','/'))" `
    "-DJP_GAME_CONFIG=$($Config.Replace('\','/'))" `
    '-DJP_CYCLE_HEADLESS=OFF' '-DJP_MODERN_ENABLE_UI=ON' `
    "-DNESRECOMP_RECOMP_UI=$($Ui.Replace('\','/'))"
Check 'Visual Studio CMake configure'
& cmake --build $Build --config Release --parallel 1
Check 'Native 5H-8F build'
& ctest --test-dir $Build -C Release --output-on-failure
Check 'Native CTest suite'
$exe = Join-Path $Build 'Release\jp_akumajou_densetsu_modern.exe'
Need $exe
$ini = Join-Path (Split-Path -Parent $exe) 'config.ini'
if (Test-Path $ini -PathType Leaf) {
    $content = [IO.File]::ReadAllText($ini)
    $updated = [regex]::Replace($content,'(?m)^(\s*SkipLauncher\s*=\s*)1(?=\s*$)','${1}0')
    if ($updated -cne $content) {
        Copy-Item $ini "$ini.before-5h7.bak" -Force
        [IO.File]::WriteAllText($ini,$updated,[Text.UTF8Encoding]::new($false))
    }
}
$env:NESRECOMP_NO_LAUNCHER = '0'
Write-Host "PASS: 5H-8F build and synthetic regressions; indexed world: $index"
Write-Host 'Event ledger captures only explicitly emitted game events; CPU RAM snapshots are observation, not entity identity proof.'
if ($Launch) {
    $diagPrefix = Join-Path $Build 'cviii-evidence-5h8f'
    $reportTool = Join-Path $tools 'cviii_diagnostic_report_5h8.py'
    Need $reportTool
    # Keep logs within a new build folder; the original state/save directories
    # and separate pre-5H7 builds are not modified.
    $trace = Join-Path $Build 'cviii-camera-5h8f.csv'
    $report = Join-Path $Build 'cviii-camera-5h8f-report.json'
    & $exe $Rom --edition enhanced --room-view 2 --room-source atlas `
        --world-index $index --asset-index $chrPack `
        --room-resources $roomPack --room-graphics $gfxPack `
        --camera-trace $trace `
        --cviii-diag-prefix $diagPrefix --cviii-diag-mode $Diagnostic
    Check 'CVIII executable'
    if (Test-Path $trace) {
        $lines = @(Get-Content -Path $trace).Count
        if ($lines -gt 1) {
            & py "-$PythonVersion" (Join-Path $tools 'cviii_camera_report_5h4.py') `
                --native $trace --out $report
            Check 'Camera evidence report'
            Write-Host "5H8E camera trace: $trace"
            Write-Host "5H8E camera rejection review: $report"
        } else {
            Write-Warning 'No gameplay frames were presented; enter gameplay from launcher to collect a trace.'
        }
    }
}

if ($Launch -and (Test-Path -LiteralPath "$diagPrefix.frames.csv")) {
    $diagReport = Join-Path $Build 'cviii-evidence-5h8f-report.json'
    & py "-$PythonVersion" $reportTool --prefix $diagPrefix --out $diagReport
    Check '5H-8 evidence integrity and summary'
    $files = @(Get-ChildItem -LiteralPath $Build -File | Where-Object {
        $_.Name -like 'cviii-evidence-5h8f.*' -or
        $_.Name -like 'cviii-camera-5h8f*' -or
        $_.Name -like 'CVIII-5H8E-Validation.*'
    })
    $files += Get-Item -LiteralPath $diagReport
    $bundle = Join-Path $Build 'CVIII-Phase5H8F-Private-Evidence.zip'
    if (Test-Path -LiteralPath $bundle) { Remove-Item -LiteralPath $bundle }
    Compress-Archive -LiteralPath @($files | Select-Object -ExpandProperty FullName -Unique) `
        -DestinationPath $bundle -CompressionLevel Optimal
    Write-Host "PRIVATE evidence bundle for upload: $bundle"
    Write-Host "Note: full mode contains owner-local mapped CHR data; do not publish the ZIP."
}
