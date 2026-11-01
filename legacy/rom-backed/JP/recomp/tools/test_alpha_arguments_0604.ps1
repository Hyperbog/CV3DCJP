# JP0604: ROM-free regression for the ACTUAL Build-Alpha.ps1 Git wrapper.
# Exercises cloning and pulling a local synthetic repository, including paths
# with spaces. Does not build or access any licensed commercial game content.
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'

$builder=Join-Path $PSScriptRoot '..\release\alpha\Build-Alpha.ps1'
$builder=(Resolve-Path -LiteralPath $builder).Path
$tokens=$null
$parseErrors=$null
$ast=[System.Management.Automation.Language.Parser]::ParseFile(
 $builder,[ref]$tokens,[ref]$parseErrors)
if ($parseErrors.Count -gt 0) {throw ($parseErrors | Out-String)}
$fnAst=$ast.Find({
 param($node)
 ($node -is [System.Management.Automation.Language.FunctionDefinitionAst]) -and
 ($node.Name -eq 'Invoke-Checked')
},$true)
if (-not $fnAst) {throw 'Actual private builder has no Invoke-Checked function'}
# Extract the function definition, not a reimplementation of the wrapper.
Invoke-Expression $fnAst.Extent.Text
$scriptContent=Get-Content -LiteralPath $builder -Raw
if ($scriptContent -match '\&\s+\$Program\s+@Args' -or
    $scriptContent -match 'Invoke-Checked\s+''git''' -or
    $scriptContent -match 'Invoke-Checked\s+''python''' -or
    ($scriptContent.Split([string[]]@('Invoke-Checked -Program'),[StringSplitOptions]::None)).Count -ne 6) {
 throw 'Private build invocations must forward five explicit named command arrays'
}
Invoke-Checked -Program 'git' -CommandArguments @('--version')
Invoke-Checked -Program 'python' -CommandArguments @('--version')

$base=Join-Path $env:RUNNER_TEMP ('JP0604-'+[Guid]::NewGuid().ToString('N'))
$origin=Join-Path $base 'Synthetic Original Source With Spaces'
$dest=Join-Path $base 'Private Clone With Spaces'
[IO.Directory]::CreateDirectory($base) | Out-Null
try {
 & git init -q $origin
 if ($LASTEXITCODE -ne 0) { throw 'Synthetic Git source init failed' }
 & git -C $origin config user.email 'jp0604@example.invalid'
 & git -C $origin config user.name 'Synthetic CI'
 Set-Content -LiteralPath (Join-Path $origin 'invented.txt') -Value 'no-retail-media'
 & git -C $origin add invented.txt
 & git -C $origin commit -qm 'Synthetic initial revision'
 if ($LASTEXITCODE -ne 0) {throw 'Synthetic commit failed'}
 & git -C $origin branch -M decompilation
 if ($LASTEXITCODE -ne 0) {throw 'Synthetic branch creation failed'}
 Invoke-Checked -Program 'git' -CommandArguments @(
  'clone','--branch','decompilation','--single-branch',$origin,$dest)
 if (-not (Test-Path -LiteralPath (Join-Path $dest 'invented.txt'))) {
  throw 'Clone did not retain all Git arguments'
 }
 Invoke-Checked -Program 'git' -CommandArguments @(
  '-C',$dest,'pull','--ff-only','origin','decompilation')
 $branch=(& git -C $dest branch --show-current).Trim()
 if ($branch -ne 'decompilation') { throw 'Clone selected wrong branch' }
 Write-Host 'PASS JP0604 actual PowerShell Git clone/pull with spaces and named argv'
} finally {
 if (Test-Path -LiteralPath $base) {
  Remove-Item -LiteralPath $base -Recurse -Force -ErrorAction SilentlyContinue
 }
}
