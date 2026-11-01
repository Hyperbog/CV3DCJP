# Isolated ROM-backed MSVC acceptance build. No changes to existing game or UI source.
[CmdletBinding()]
param(
    [ValidateSet('Validate','Build','Test','Launch')][string]$Action='Validate',
    [string]$Rom='C:\NESDev\AkumajouDensetsu\Akumajou Densetsu (Japan).nes',
    [string]$Engine='C:\NESDev\nesrecomp',
    [string]$BuildDir='C:\NESDev\CV3DCJP-Builds\5h8f-original',
    [string]$Generator='Visual Studio 18 2026',
    [string]$GameConfig=''
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'

function Need([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing file: $Path" }
}
function Check([string]$Name) {
    if ($LASTEXITCODE -ne 0) { throw "$Name failed (exit $LASTEXITCODE)" }
}
function Canonical([string]$Path) {
    return [IO.Path]::GetFullPath($Path).TrimEnd([char]'\',[char]'/')
}

$repo=Canonical (Join-Path $PSScriptRoot '..\..')
$project=Join-Path $repo 'legacy\rom-backed\JP\recomp\native\nesrecomp_modern'
$engine=Canonical $Engine
$rom=Canonical $Rom
$build=Canonical $BuildDir
foreach ($unsafe in @($repo,$engine,'C:\NESDev\CVIII-Decomp','C:\NESDev\AkumajouDensetsu')) {
    $protected=Canonical $unsafe
    if ($build.Equals($protected,[StringComparison]::OrdinalIgnoreCase) -or
        $build.StartsWith($protected+'\',[StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to overwrite source or working game: $build"
    }
}
if ($build.Length -lt 12) { throw 'Build directory too broad' }
Need (Join-Path $project 'CMakeLists.txt')
Need (Join-Path $engine 'runner\cyc\project.cmake')
Need $rom
if ($GameConfig) { Need $GameConfig }
$expected='958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28'
$actual=(Get-FileHash -LiteralPath $rom -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actual -ne $expected) { throw "Wrong ROM SHA-256: $actual" }
if ((Get-Item -LiteralPath $rom).Length -ne 393232) { throw 'Wrong ROM size' }
$null=Get-Command cmake -ErrorAction Stop
$null=Get-Command ctest -ErrorAction Stop
if ($Action -in @('Validate','Build')) {
    $generators=(& cmake --help) -join [Environment]::NewLine
    Check 'CMake generator inventory'
    if (-not $generators.Contains($Generator)) {
        throw "CMake generator unavailable: $Generator"
    }
}
Write-Host "ROM verified: $actual"
Write-Host "Source: $project"
Write-Host "External engine: $engine"
Write-Host "Isolated build: $build"
Write-Host 'Private recomp-ui: disabled; no UI source patches'
if ($Action -eq 'Validate') { return }

$marker=Join-Path $build '.cv3dcjp-isolated-5h8f'
$markerValue='CV3DCJP-5H8F-ISOLATED-v1'
if (Test-Path -LiteralPath $build) {
    if (-not (Test-Path -LiteralPath $marker -PathType Leaf)) {
        throw "Existing unmarked build directory: $build"
    }
    if ((Get-Content -LiteralPath $marker -Raw).Trim() -cne $markerValue) {
        throw "Build ownership marker mismatch: $marker"
    }
} elseif ($Action -eq 'Build') {
    New-Item -ItemType Directory -Path $build -Force | Out-Null
    [IO.File]::WriteAllText($marker,$markerValue+[Environment]::NewLine,
        [Text.UTF8Encoding]::new($false))
} else { throw "No isolated build yet. Run -Action Build first." }

if ($Action -eq 'Build') {
    $opts=@('-S',$project,'-B',$build,'-G',$Generator,'-A','x64',
        "-DNESRECOMP_EXTERNAL_ROOT=$($engine.Replace('\','/'))",
        "-DJP_ORIGINAL_JP_ROM=$($rom.Replace('\','/'))",
        '-DJP_CYCLE_HEADLESS=OFF','-DJP_MODERN_ENABLE_UI=OFF')
    if ($GameConfig) {
        $config=Canonical $GameConfig
        $opts+="-DJP_GAME_CONFIG=$($config.Replace('\','/'))"
    }
    & cmake @opts
    Check 'CMake configure'
    & cmake --build $build --config Release --parallel 1
    Check 'MSVC ROM-backed build'
}
if ($Action -in @('Build','Test')) {
    & ctest --test-dir $build -C Release --output-on-failure
    Check 'Native CTest'
    Write-Host 'PASS: build and tests only; live ROM gameplay remains unverified.'
    return
}
$exe=Join-Path $build 'Release\jp_akumajou_densetsu_modern.exe'
Need $exe
$env:NESRECOMP_NO_LAUNCHER='1'
Write-Host 'Launching original-ROM mode from isolated build'
& $exe $rom --edition original --room-view 0
Check 'Original-ROM game'
