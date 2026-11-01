# JP0607 Windows Python probe regression, independent of any game ROM.
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'

$helper=Join-Path $PSScriptRoot '..\release\alpha\Resolve-Python.ps1'
$helper=(Resolve-Path -LiteralPath $helper).Path
$tokens=$null
$errs=$null
[System.Management.Automation.Language.Parser]::ParseFile(
 $helper,[ref]$tokens,[ref]$errs) | Out-Null
if ($errs.Count -gt 0) { throw ($errs | Out-String) }
. $helper

$root=Join-Path $env:RUNNER_TEMP ('JP0607 python alias '+[Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
try {
 # Recreate the exact "python.exe" Store-alias result without using Python:
 # a callable process exists but exits 9009 when asked to run -c.
 $alias=Join-Path $root 'fake-python-alias.cmd'
 Set-Content -LiteralPath $alias -Encoding Ascii -Value @(
  '@echo off',
  'echo Python was not found; use Microsoft Store >&2',
  'exit /b 9009'
 )
 if (Test-CviiiPython -Executable $alias -Prefix @()) {
  throw 'Windows Store-style Python alias was mistakenly accepted'
 }

 # The selected real interpreter must execute Python >= 3.10.
 $working=Resolve-CviiiPython
 if (-not (Test-CviiiPython -Executable $working.Executable -Prefix $working.Prefix)) {
  throw 'Resolved interpreter cannot execute a Python 3.10+ script'
 }
 $source=Join-Path $root 'Synthetic script with spaces.py'
 $stamp=Join-Path $root 'Synthetic output with spaces.txt'
 Set-Content -LiteralPath $source -Encoding Ascii -Value @(
  'import sys, pathlib',
  'pathlib.Path(sys.argv[1]).write_text("ok:" + sys.argv[2], encoding="utf-8")'
 )
 Invoke-CviiiPython -Script $source -Arguments @($stamp,'arg spaces')
 if ((Get-Content -LiteralPath $stamp -Raw) -ne 'ok:arg spaces') {
  throw 'Python script arguments were not forwarded correctly'
 }
 Write-Host ('PASS JP0607: rejected alias 9009; real interpreter '+
             $working.Executable+' completed spaced-path script')
} finally {
 Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
