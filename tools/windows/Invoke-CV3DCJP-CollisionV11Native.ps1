# Owner-local, fail-closed collision v11.2 original-CPU diagnostic run.
# No modification to protected 5h8f-original baseline or external NESRecomp.
[CmdletBinding()]
param(
  [ValidateSet('BuildAndRun','Build','Run')][string]$Action='BuildAndRun',
  [Parameter(Mandatory=$true)][string]$GameConfig,
  [Parameter(Mandatory=$true)][string]$InputDir,
  [string]$Rom='C:\NESDev\AkumajouDensetsu\Akumajou Densetsu (Japan).nes',
  [string]$Engine='C:\NESDev\nesrecomp',
  [string]$Repo='C:\NESDev\CV3DCJP-Repo',
  [string]$BuildDir='C:\NESDev\CV3DCJP-Builds\collision-v11-paired',
  [string]$Generator='Visual Studio 18 2026',
  [ValidateSet(0,1,2)][int]$Shift=0,
  [ValidateRange(985,10000)][int]$Frames=1000,
  [ValidateRange(0,3)][int]$Align=0
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$expectedRom='958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28'
$expectedUpstream='2b4fcdb98c9e702be3fb365cfc645003c50b5fc1'
$markValue='CV3DCJP-COLLISION-V11-DIAGNOSTIC-ISOLATED-v1'

function Ensure-File([string]$path) {
  if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing file: $path" }
}
function Exit-Check([string]$label) {
  if ($LASTEXITCODE -ne 0) { throw "$label failed with exit code $LASTEXITCODE" }
}
function Safe-Path([string]$path) {
  return [IO.Path]::GetFullPath($path).TrimEnd('\','/')
}

$rom=Safe-Path $Rom
$repo=Safe-Path $Repo
$engine=Safe-Path $Engine
$baseGame=Safe-Path $GameConfig
$inputDir=Safe-Path $InputDir
$build=Safe-Path $BuildDir
$project=Join-Path $repo 'legacy\rom-backed\JP\recomp\native\nesrecomp_modern'
$hooks=Join-Path $repo 'tools\instrumentation\collision_v11_hook_sites.toml'
$input=Join-Path $inputDir "cv3dcjp-native-input-UNVERIFIED-shift-$Shift.txt"
$manifest=Join-Path $inputDir 'cv3dcjp-native-input-manifest.json'
foreach($path in @($rom,$baseGame,$hooks,$input,$manifest,(Join-Path $project 'CMakeLists.txt'),(Join-Path $engine 'runner\cyc\project.cmake'))) {
  Ensure-File $path
}
if ((Get-Item -LiteralPath $rom).Length -ne 393232 -or
    (Get-FileHash -LiteralPath $rom -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedRom) {
  throw 'Wrong original Japanese ROM. No execution permitted.'
}
$hash=(Get-FileHash -LiteralPath $input -Algorithm SHA256).Hash.ToLowerInvariant()
$receipt=Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
# Two manifest layouts were released during phase 1. The earlier
# owner ZIP uses mapping_proven; the repository converter uses
# start_phase_alignment_proven. Require the *matching schema and explicit
# false* instead of reading an absent property under StrictMode.
$unverifiedField = switch ([string]$receipt.schema) {
  'cv3dcjp-v11.2-nesrecomp-input-CANDIDATES' { 'mapping_proven' }
  'cv3dcjp-v11.2-native-input-unverified-candidates' { 'start_phase_alignment_proven' }
  default { throw "Unknown input manifest schema: $($receipt.schema)" }
}
$flagProperty=$receipt.PSObject.Properties[$unverifiedField]
if ($null -eq $flagProperty -or $flagProperty.Value -isnot [bool] -or
    $flagProperty.Value) {
  throw 'Input manifest does not explicitly mark input alignment unverified'
}
$selected=@($receipt.candidate_schedules | Where-Object { $_.shift -eq $Shift })
if ($selected.Count -ne 1 -or [string]$selected[0].sha256 -ne $hash -or
    [string]$receipt.source_rom_sha256 -ne $expectedRom) {
  throw 'Input manifest does not match selected candidate or original ROM.'
}
$expectedInputName = if ($selected[0].PSObject.Properties['relative_filename']) {
    [string]$selected[0].relative_filename
  } elseif ($selected[0].PSObject.Properties['name']) {
    [string]$selected[0].name
  } else { throw 'Input manifest is missing selected schedule filename' }
if ($expectedInputName -cne [IO.Path]::GetFileName($input)) {
  throw 'Input manifest names a different schedule file'
}
if (!(Get-Command git -ErrorAction SilentlyContinue)) { throw 'git not installed' }
$head=(& git -C $engine rev-parse HEAD).Trim()
Exit-Check 'Read NESRecomp revision'
if ($head -ne $expectedUpstream) { throw "NESRecomp HEAD $head is NOT the pinned tested revision" }
foreach($forbidden in @($repo,$engine,'C:\NESDev\AkumajouDensetsu','C:\NESDev\CV3DCJP-Builds\5h8f-original')) {
  $protected=Safe-Path $forbidden
  if ($build.Equals($protected,[StringComparison]::OrdinalIgnoreCase) -or
      $build.StartsWith($protected+'\', [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing protected build location: $build"
  }
}
$marker=Join-Path $build '.cv3dcjp-collision-v11-private'
if (Test-Path -LiteralPath $build) {
  Ensure-File $marker
  if ((Get-Content -LiteralPath $marker -Raw).Trim() -cne $markValue) {
    throw 'Unrecognized diagnostic build folder'
  }
} elseif ($Action -ne 'Run') {
  New-Item -ItemType Directory -Path $build -Force | Out-Null
  [IO.File]::WriteAllText($marker,$markValue+[Environment]::NewLine,[Text.UTF8Encoding]::new($false))
} else { throw 'Diagnostic build missing; first run -Action BuildAndRun' }
$private=Join-Path $build 'private'
if (!(Test-Path -LiteralPath $private)) { New-Item -ItemType Directory -Path $private | Out-Null }
$diagnosticConfig=Join-Path $private 'game-v11-observer.toml'
$raw=Get-Content -LiteralPath $baseGame -Raw
$snippet=Get-Content -LiteralPath $hooks -Raw
$identifiers=[regex]::Matches($snippet,'(?m)^id\s*=\s*"([^"]+)"')
if ($identifiers.Count -ne 5) { throw 'Unexpected number of pinned collision hook IDs' }
$existing=0
foreach($id in $identifiers){
  if ($raw.Contains($id.Groups[1].Value)) { $existing++ }
}
if ($existing -ne 0 -and $existing -ne 5) {
  throw 'Owner config contains an incomplete collision observer hook set'
}
if ($existing -eq 0) { $raw += [Environment]::NewLine+$snippet }
[IO.File]::WriteAllText($diagnosticConfig,$raw,[Text.UTF8Encoding]::new($false))

if ($Action -in @('Build','BuildAndRun')) {
  $gen=(& cmake --help) -join [Environment]::NewLine
  Exit-Check 'Read CMake generators'
  if (!$gen.Contains($Generator)) { throw "Unavailable generator: $Generator" }
  $options=@('-S',$project,'-B',$build,'-G',$Generator,'-A','x64',
     "-DNESRECOMP_EXTERNAL_ROOT=$($engine.Replace('\','/'))",
     "-DJP_ORIGINAL_JP_ROM=$($rom.Replace('\','/'))",
     "-DJP_GAME_CONFIG=$($diagnosticConfig.Replace('\','/'))",
     '-DJP_COLLISION_V11_SHADOW=ON','-DJP_CYCLE_HEADLESS=ON',
     '-DJP_MODERN_ENABLE_UI=OFF')
  & cmake @options
  Exit-Check 'Configure isolated collision observer'
  & cmake --build $build --config Release --parallel 1
  Exit-Check 'Build isolated collision observer'
  & ctest --test-dir $build -C Release --output-on-failure
  Exit-Check 'Isolated collision native CTest'
}
if ($Action -eq 'Build') { Write-Host 'Diagnostic build complete. No runtime parity is claimed.'; return }
$exe=Join-Path $build 'Release\jp_akumajou_densetsu_modern.exe'
Ensure-File $exe
$session=Join-Path $build ('captures\shift-'+$Shift+'-align-'+$Align+'-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
if (Test-Path -LiteralPath $session) { throw "Output directory exists: $session" }
New-Item -ItemType Directory -Path $session -Force | Out-Null
$trace=Join-Path $session 'native-fcde.csv'
$dma=Join-Path $session 'native-dma.csv'
$busCsv=Join-Path $session 'native-dma-bus-v12.csv'
$hashFile=Join-Path $session 'native-hashes.txt'
$bus=Join-Path $session 'native-cycle-frame-IRQ-window.txt'
$log=Join-Path $session 'native-console.log'
# IRQ sample #101 is near Mesen inputPoll frame 964. This is only a probe.
$irqCandidateFrame=964-2+$Shift
$argsNative=@($rom,'--edition','original','--room-view','0','--headless',
  '--no-save','--frames',"$Frames",'--input',$input,
  '--hash-out',$hashFile,'--trace-frame',"$irqCandidateFrame",'--trace-out',$bus,
  '--align',"$Align",'--collision-v11-rom',$rom,
  '--collision-v11-trace',$trace,'--collision-v11-dma-trace',$dma,
  '--collision-v11-bus-trace',$busCsv)
Write-Host "RUNNING UNVERIFIED candidate frame shift $Shift, PPU align $Align"
Write-Host "Private outputs: $session"
& $exe @argsNative *> $log
Exit-Check 'First native collision diagnostic run'
Ensure-File $trace
Ensure-File $dma
Ensure-File $busCsv
$cpuRows=@(Import-Csv -LiteralPath $trace)
$dmaRows=@(Import-Csv -LiteralPath $dma)
$summary=[ordered]@{schema='cv3dcjp-v11.2-owner-native-execution-receipt';
  observed_native_executed=$true; input_shift=$Shift; ppu_align=$Align;
  frame_alignment_proven=$false; bus_cycle_parity_proven=$false;
  binary_sha256=(Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant();
  rom_sha256=$expectedRom; input_sha256=$hash;
  native_cpu_samples=$cpuRows.Count; native_dma_samples=$dmaRows.Count;
  native_dma_bus_sha256=(Get-FileHash $busCsv -Algorithm SHA256).Hash.ToLowerInvariant();
  hardware_event_stream_generated=$true;
  expected_mesen_capture_samples=128; captured_irq_trace_frame=$irqCandidateFrame;
  source_engine_commit=$head }
$summary | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $session 'run-receipt.json') -Encoding UTF8
if ($cpuRows.Count -eq 0 -or $dmaRows.Count -eq 0) {
  throw "Native game ran, but observer recorded zero collision pairs; inspect $log"
}
Write-Host "Native CPU pairs: $($cpuRows.Count); DMA pairs: $($dmaRows.Count)"
Write-Host "UPLOAD private capture directory ZIP after inspecting it: $session"
Write-Host 'No native timing parity has been established by execution alone.'
