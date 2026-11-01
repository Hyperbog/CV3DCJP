# Capture CPU selector writes and frame-end PPU in ONE isolated original-ROM run.
[CmdletBinding()]
param(
    [string]$Rom = 'C:\NESDev\AkumajouDensetsu\Akumajou Densetsu (Japan).nes',
    [string]$Engine = 'C:\NESDev\nesrecomp',
    [string]$BuildDir = 'C:\NESDev\CV3DCJP-Builds\5h8f-original',
    [switch]$ValidateOnly
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$root = Join-Path $repo 'data\generated\stage0-sync-v3'
function Need([string]$p) {
    if (-not (Test-Path -LiteralPath $p -PathType Leaf)) {
        throw "Missing required file: $p"
    }
}
$romHash = '958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28'
Need $Rom
if ((Get-FileHash -LiteralPath $Rom -Algorithm SHA256).Hash.ToLowerInvariant() -ne $romHash) {
    throw 'Authenticated Japanese ROM SHA-256 mismatch'
}
$originalExe = Join-Path $BuildDir 'Release\jp_akumajou_densetsu_modern.exe'
$hookSource = Join-Path $Engine 'runner\cyc\hw_machine.c'
Need $originalExe
Need $hookSource
$sourceText = [IO.File]::ReadAllText($hookSource)
if (-not $sourceText.Contains('cyc_cviii_selector_write_5h8u') -or
    -not $sourceText.Contains('cviii_u_fetch_cycle')) {
    throw '5H8U CPU cycle hook is absent from the engine; do not claim synchronization.'
}
$hostSource = Join-Path $repo 'legacy\rom-backed\JP\recomp\native\nesrecomp_modern\cviii_selector_native_5h8u.c'
Need $hostSource
if (-not ([IO.File]::ReadAllText($hostSource).Contains('CVIII_SELECTOR_TRACE_5H8U'))) {
    throw 'The host lacks the matching 5H8U observer'
}
Write-Host "Verified ROM: $romHash"
Write-Host "Unmodified executable source: $originalExe"
Write-Host "Private output root: $root"
if ($ValidateOnly) { return }

$runId = [Guid]::NewGuid().ToString('N')
$runDir = Join-Path $root $runId
New-Item -ItemType Directory -Path $runDir -ErrorAction Stop | Out-Null
# Run from a disposable COPY, protecting saves/config in the working baseline.
$runExeDir = Join-Path $runDir 'executable'
Copy-Item -LiteralPath (Join-Path $BuildDir 'Release') -Destination $runExeDir -Recurse
$exe = Join-Path $runExeDir 'jp_akumajou_densetsu_modern.exe'
Need $exe
$prefix = Join-Path $runDir 'cviii-evidence'
$trace = Join-Path $runDir 'selector-cycle.csv'
$oldTrace = [Environment]::GetEnvironmentVariable('CVIII_SELECTOR_TRACE_5H8U','Process')
$oldLauncher = [Environment]::GetEnvironmentVariable('NESRECOMP_NO_LAUNCHER','Process')
$started = [DateTimeOffset]::UtcNow.ToString('o')
$gameArgs = @($Rom,'--edition','original','--room-view','0',
              '--cviii-diag-prefix',$prefix,'--cviii-diag-mode','frames')
try {
    $env:CVIII_SELECTOR_TRACE_5H8U = $trace
    $env:NESRECOMP_NO_LAUNCHER = '1'
    & $exe @gameArgs
    $gameExit = $LASTEXITCODE
} finally {
    [Environment]::SetEnvironmentVariable('CVIII_SELECTOR_TRACE_5H8U',$oldTrace,'Process')
    [Environment]::SetEnvironmentVariable('NESRECOMP_NO_LAUNCHER',$oldLauncher,'Process')
}
if ($gameExit -ne 0) {
    throw "Game exited with code $gameExit; evidence kept: $runDir"
}
$files = @{
    'frames.csv' = "$prefix.frames.csv"
    'palette.bin' = "$prefix.palette.bin"
    'selector-cycle.csv' = $trace
}
$hashes = @{}
foreach ($name in $files.Keys) {
    Need $files[$name]
    if ((Get-Item -LiteralPath $files[$name]).Length -le 0) {
        throw "Empty required capture: $($files[$name])"
    }
    $hashes[$name] = (Get-FileHash -LiteralPath $files[$name] -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($name -ne 'selector-cycle.csv') {
        Move-Item -LiteralPath $files[$name] -Destination (Join-Path $runDir $name)
    }
}
$receipt = [ordered]@{
    schema = 'cv3dcjp.synchronized-observations/v3'
    run_id = $runId
    rom_sha256 = $romHash
    executable_sha256 = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
    started_utc = $started
    finished_utc = [DateTimeOffset]::UtcNow.ToString('o')
    exit_code = 0
    hook_source_sha256 = (Get-FileHash -LiteralPath $hookSource -Algorithm SHA256).Hash.ToLowerInvariant()
    sha256 = $hashes
}
$receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $runDir 'session.json') -Encoding utf8
$validator = Join-Path $repo 'tools\extract\verify_stage0_synchronized_v3.py'
& py -3.12 $validator --rom $Rom --session $runDir --out (Join-Path $runDir 'audit.json')
if ($LASTEXITCODE -ne 0) {
    throw "Synchronization check incomplete; private evidence kept: $runDir"
}
Write-Host "PASS: same-run CPU selector and frame-end PPU evidence: $runDir"
Write-Host 'Palette PRG source attribution remains unverified.'
