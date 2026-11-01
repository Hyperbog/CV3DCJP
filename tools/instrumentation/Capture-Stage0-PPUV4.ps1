# One-process Stage 0 PPU palette, VRC6 CHR and CPU-source context capture.
[CmdletBinding()]
param(
    [ValidateSet('Validate','Prepare','Build','Capture')]
    [string]$Action = 'Validate',
    [string]$OriginalEngine = 'C:\NESDev\nesrecomp',
    [string]$ProbeEngine = 'C:\NESDev\nesrecomp-ppu-v4',
    [string]$BuildDir = 'C:\NESDev\CV3DCJP-Builds\stage0-ppu-v4',
    [string]$Rom = 'C:\NESDev\AkumajouDensetsu\Akumajou Densetsu (Japan).nes'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$prepare = Join-Path $PSScriptRoot 'prepare_ppu_probe_v4.py'
$baseline = Join-Path $repo 'tools\windows\Build-CV3DCJP-IsolatedBaseline.ps1'

function Need([string]$path) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required file missing: $path"
    }
}
function Check([string]$phase) {
    if ($LASTEXITCODE -ne 0) {
        throw "$phase returned exit code $LASTEXITCODE"
    }
}
Need $prepare
Need $baseline
Need $Rom
$romsha = '958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28'
if ((Get-FileHash -LiteralPath $Rom -Algorithm SHA256).Hash.ToLowerInvariant() -ne $romsha) {
    throw 'Unexpected Japanese ROM identity; refusing execution'
}
if ($Action -in @('Validate','Prepare')) {
    & py -3.12 $prepare --source $OriginalEngine --destination $ProbeEngine --check-only
    Check 'Original engine source check'
    if ($Action -eq 'Validate') { return }
    & py -3.12 $prepare --source $OriginalEngine --destination $ProbeEngine
    Check 'Isolated engine copy/patch'
    return
}
Need (Join-Path $ProbeEngine 'runner\cyc\cviii_ppu_probe_v4.h')
if ($Action -eq 'Build') {
    & $baseline -Action Build -Engine $ProbeEngine -BuildDir $BuildDir -Rom $Rom
    if (-not $?) { throw 'Isolated MSVC build or native CTest failed' }
    return
}
$sourceExe = Join-Path $BuildDir 'Release\jp_akumajou_densetsu_modern.exe'
Need $sourceExe
$root = Join-Path $repo 'data\generated\stage0-ppu-v4'
$runID = [guid]::NewGuid().ToString('N')
$run = Join-Path $root $runID
New-Item -ItemType Directory -Path $root -Force -ErrorAction Stop | Out-Null
New-Item -ItemType Directory -Path $run -ErrorAction Stop | Out-Null
# Run from disposable copy; preserve existing successful original-mode baseline.
$bin = Join-Path $run 'executable'
Copy-Item -LiteralPath (Join-Path $BuildDir 'Release') -Destination $bin -Recurse
$exe = Join-Path $bin 'jp_akumajou_densetsu_modern.exe'
Need $exe
$events = Join-Path $run 'ppu-v4-events.csv'
$selectors = Join-Path $run 'selector-cycle.csv'
$prefix = Join-Path $run 'cviii-evidence'
$keys = @('CVIII_PPU_PROBE_V4','CVIII_SELECTOR_TRACE_5H8U','NESRECOMP_NO_LAUNCHER')
$previous = @{}
foreach ($key in $keys) {
    $previous[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
}
try {
    $env:CVIII_PPU_PROBE_V4 = $events
    $env:CVIII_SELECTOR_TRACE_5H8U = $selectors
    $env:NESRECOMP_NO_LAUNCHER = '1'
    & $exe $Rom --edition original --room-view 0 --cviii-diag-prefix $prefix --cviii-diag-mode frames
    $code = $LASTEXITCODE
} finally {
    foreach ($key in $keys) {
        [Environment]::SetEnvironmentVariable($key, $previous[$key], 'Process')
    }
}
if ($code -ne 0) {
    throw "Game exited with code $code; preserve evidence: $run"
}
$files = [ordered]@{
    'ppu-v4-events.csv' = $events
    'selector-cycle.csv' = $selectors
    'frames.csv' = "$prefix.frames.csv"
    'palette.bin' = "$prefix.palette.bin"
    'lines.bin' = "$prefix.lines.bin"
    'room-events.csv' = "$prefix.events.csv"
}
$digests = @{}
foreach ($key in $files.Keys) {
    Need $files[$key]
    if ((Get-Item -LiteralPath $files[$key]).Length -eq 0) {
        throw "Empty required capture: $key"
    }
    $digests[$key] = (Get-FileHash -LiteralPath $files[$key] -Algorithm SHA256).Hash.ToLowerInvariant()
}
$receipt = [ordered]@{
    schema = 'cv3dcjp.ppu-event-trace/v4'
    run_id = $runID
    rom_sha256 = $romsha
    executable_sha256 = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
    probe_source_sha256 = (Get-FileHash -LiteralPath $prepare -Algorithm SHA256).Hash.ToLowerInvariant()
    files_sha256 = $digests
    cpu_fetch_context_is_not_palette_commit_producer_proof = $true
}
[IO.File]::WriteAllText((Join-Path $run 'session-v4.json'),
    ($receipt | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
Write-Host "PRIVATE_CAPTURE: $run"
Write-Host 'V4 collected raw events; causal palette source and world geometry remain unverified.'
