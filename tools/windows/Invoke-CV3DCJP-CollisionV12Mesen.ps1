# v12 private MesenCE DMA bus capture. No changes to installed emulator/game.
[CmdletBinding()]
param(
 [ValidateSet('Prepare','Build','Run')][string]$Action='Prepare',
 [string]$Repo='C:\NESDev\CV3DCJP-Repo',
 [string]$PrivateMesen='C:\NESDev\mesence-collision-v12-pin',
 [string]$Rom='C:\NESDev\AkumajouDensetsu\Akumajou Densetsu (Japan).nes',
 [string]$InputDir="$env:USERPROFILE\Downloads\CV3DCJP-Collision-v11-Replay",
 [string]$CaptureRoot='C:\NESDev\CV3DCJP-Builds\collision-v12-mesen'
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$pin='4b8a196e299b3de297b204fd1208d2981562c89b'
$romSha='958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28'
function Need($p) {
 if (!(Test-Path -LiteralPath $p -PathType Leaf)) { throw "Missing file: $p" }
}
function Check($name) {
 if ($LASTEXITCODE -ne 0) { throw "$name failed: $LASTEXITCODE" }
}
$repo=[IO.Path]::GetFullPath($Repo)
$private=[IO.Path]::GetFullPath($PrivateMesen)
$rom=[IO.Path]::GetFullPath($Rom)
$root=[IO.Path]::GetFullPath($CaptureRoot)
$patch=Join-Path $repo 'tools\instrumentation\prepare_mesen_dmc_bus_v12.py'
$lua=Join-Path $repo 'tools\instrumentation\mesen2_collision_fcde_sync_dmc_bus_v12.lua'
$inputs=Join-Path $InputDir 'cv3dcjp-native-input-UNVERIFIED-shift-1.txt'
$manifest=Join-Path $InputDir 'cv3dcjp-native-input-manifest.json'
foreach($p in @($rom,$patch,$lua)) { Need $p }
if ((Get-Item $rom).Length -ne 393232 -or
    (Get-FileHash $rom -Algorithm SHA256).Hash.ToLowerInvariant() -ne $romSha) {
 throw 'Wrong original Japanese ROM; refusing to run'
}
foreach($protected in @($repo,'C:\NESDev\nesrecomp','C:\NESDev\AkumajouDensetsu')) {
 $safe=[IO.Path]::GetFullPath($protected).TrimEnd('\')
 if ($private.Equals($safe,[StringComparison]::OrdinalIgnoreCase) -or
     $private.StartsWith($safe+'\', [StringComparison]::OrdinalIgnoreCase)) {
   throw "Refusing protected checkout: $private"
 }
}
if ($Action -eq 'Prepare') {
 if (Test-Path $private) { throw "Private Mesen source exists: $private" }
 & git clone --no-checkout https://github.com/nesdev-org/MesenCE.git $private
 Check 'Private MesenCE Git clone'
 & git -C $private checkout --detach $pin
 Check 'Pinned MesenCE checkout'
 & py -3 $patch --private-source $private
 Check 'Pinned MesenCE source patch'
 Write-Host "Private source prepared: $private"
 return
}
Need (Join-Path $private 'Mesen.sln')
$head=(& git -C $private rev-parse HEAD).Trim()
Check 'Read private MesenCE HEAD'
if ($head -ne $pin) { throw "Incorrect private MesenCE HEAD: $head" }
$patched=Get-Content -LiteralPath (Join-Path $private 'Core\NES\Debugger\NesDebugger.cpp') -Raw
if (!$patched.Contains('CV3DCJP_MESEN_DMC_CSV')) {
 throw 'Private MesenCE lacks the source-identified DMA debugger patch'
}
if ($Action -eq 'Build') {
 $cmd=Get-Command MSBuild.exe -ErrorAction SilentlyContinue
 $msbuild=if ($null -ne $cmd) { $cmd.Source } else { $null }
 if (!$msbuild) {
   $vswhere='C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
   Need $vswhere
   $found=@(& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\Current\Bin\MSBuild.exe')
   if ($found.Count -gt 0) { $msbuild=$found[0] }
 }
 if (!$msbuild) { throw 'MSBuild.exe not available from Visual Studio' }
 & $msbuild (Join-Path $private 'Mesen.sln') '/t:Build' '/p:Configuration=Release' '/p:Platform=x64' '/m:1' '/verbosity:minimal'
 Check 'MesenCE patched Release x64 build'
 Write-Host 'Private MesenCE build complete. No gameplay capture yet.'
 return
}
foreach($p in @($inputs,$manifest)) { Need $p }
$receipt=Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
$selected=@($receipt.candidate_schedules | Where-Object { $_.shift -eq 1 })
if ($selected.Count -ne 1 -or
    $selected[0].sha256 -ne (Get-FileHash $inputs -Algorithm SHA256).Hash.ToLowerInvariant()) {
 throw 'Private input schedule does not match shift-1 manifest'
}
$exe=@(Get-ChildItem $private -Recurse -Filter Mesen.exe -File |
       Where-Object { $_.FullName -notmatch '[\\/](?:obj|packages)[\\/]' } |
       Sort-Object LastWriteTime -Descending)
if ($exe.Count -eq 0) { throw 'Patched Mesen.exe missing; use -Action Build first' }
$bin=$exe[0].FullName
$session=Join-Path $root ('session-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
if (Test-Path $session) { throw 'Capture directory exists' }
New-Item -ItemType Directory -Path $session -Force | Out-Null
$env:CV3DCJP_MESEN_DMC_CSV=Join-Path $session 'mesen-real-dma-bus-v12.csv'
$env:CV3DCJP_MESEN_OUTPUT_DIR=$session
$env:CV3DCJP_MESEN_INPUT=(Resolve-Path -LiteralPath $inputs).Path
Write-Host 'Open the ORIGINAL Japanese ROM in this private Mesen build.'
Write-Host 'Enable Allow access to I/O and OS functions in Lua Settings.'
Write-Host "Run Lua script: $lua"
Write-Host 'Reset the emulator AFTER starting script; play until 128 calls.'
Write-Host 'Stop the Lua script and close the private Mesen emulator.'
Write-Host "Private evidence directory: $session"
& $bin
Check 'Private MesenCE process'
$csv=@(Get-ChildItem $session -File -Filter '*.csv')
$summary=[ordered]@{
 schema='cv3dcjp-collision-v12-owner-mesen-debugger-run'
 Mesen_git_commit=$pin
 Mesen_binary_sha256=(Get-FileHash $bin -Algorithm SHA256).Hash
 rom_sha256=$romSha
 input_sha256=(Get-FileHash $inputs -Algorithm SHA256).Hash
 private_capture_directory=$session
 files=@($csv | ForEach-Object { $_.Name })
 source_verified_dmc_debugger_patch_present=$true
 hardware_parity_proven=$false
 original_cpu_bytes_retired=0
}
$summary | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $session 'mesen-run-receipt.json') -Encoding UTF8
if ($csv.Count -lt 4) {
 throw "Incomplete capture ($($csv.Count) CSVs). Check Lua/reset: $session"
}
Write-Host "Private evidence capture ready for ZIP: $session"
