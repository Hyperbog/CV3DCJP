# CV3DCJP: delete GitHub branches ONLY after verified merged PR and SHA guard.
# Default: read-only dry-run. Use -Execute for remote deletion after reviewing.
[CmdletBinding()]
param(
  [string]$Repo = 'C:\NESDev\CV3DCJP-Repo',
  [switch]$Execute
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$manifest = Join-Path $root 'docs\repository\merged-branch-inventory.json'
if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
  throw "Missing audit manifest: $manifest"
}
$inventory = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
if ($inventory.repository -cne 'Hyperbog/CV3DCJP' -or
    $inventory.default_branch -cne 'main') {
  throw 'Unexpected repository identity or default branch'
}
if (-not (Test-Path -LiteralPath (Join-Path $Repo '.git'))) {
  throw "Expected a Git checkout at $Repo"
}
function Git([string[]]$Argv) {
  $out = & git -C $Repo @Argv
  if ($LASTEXITCODE -ne 0) { throw "git $($Argv -join ' ') failed ($LASTEXITCODE)" }
  return $out
}
$origin = (Git -Argv @('remote','get-url','origin') | Select-Object -First 1).Trim()
if ($origin -notmatch '^(https://github\.com/|git@github\.com:|ssh://git@github\.com/|git://github\.com/)Hyperbog/CV3DCJP(\.git)?$') {
  throw "Refusing remote that is not Hyperbog/CV3DCJP: $origin"
}
$mainRefs = @(Git -Argv @('ls-remote','--heads','origin','refs/heads/main'))
if ($mainRefs.Count -ne 1) { throw 'Cannot verify one remote main branch' }
Write-Host "Repository: $origin"
Write-Host "Protected branch: main ($($mainRefs[0].Split("`t")[0]))"
if (-not $Execute) { Write-Host 'DRY RUN: no branches will be deleted.' }
else { Write-Host 'EXECUTE: deleting only SHA-guarded, PR-merged branches.' }
$removed = 0
$skipped = 0
foreach ($item in @($inventory.branches)) {
  $branch = [string]$item.name
  $expected = [string]$item.expected_sha
  $number = [int]$item.merged_pr
  if ($branch -eq 'main' -or $branch -match '(^|/)\.\.' -or
      $branch -notmatch '^[A-Za-z0-9][A-Za-z0-9._/-]*$' -or
      $expected -notmatch '^[0-9a-f]{40}$' -or $number -lt 1) {
    throw "Invalid or protected branch record: $branch"
  }
  $refs = @(Git -Argv @('ls-remote','--heads','origin',"refs/heads/$branch"))
  if ($refs.Count -eq 0) {
    Write-Host "ALREADY ABSENT: $branch"
    continue
  }
  if ($refs.Count -ne 1) { throw "Unexpected ref count for $branch" }
  $remoteSha = $refs[0].Split("`t")[0].ToLowerInvariant()
  if ($remoteSha -cne $expected) {
    Write-Warning "SKIP: $branch moved from recorded SHA. No deletion."
    $skipped++
    continue
  }
  $url = "https://api.github.com/repos/Hyperbog/CV3DCJP/pulls/$number"
  try {
    $pr = Invoke-RestMethod -Method Get -Uri $url `
      -Headers @{'User-Agent'='CV3DCJP-merged-branch-pruner';'Accept'='application/vnd.github+json'}
  } catch {
    Write-Warning "SKIP: cannot confirm PR #$number (API unavailable/rate limit). $_"
    $skipped++
    continue
  }
  if (-not $pr.merged_at -or $pr.state -ne 'closed' -or
      $pr.base.ref -ne 'main' -or $pr.head.sha -ne $expected) {
    Write-Warning "SKIP: PR #$number does not prove merged branch $branch at $expected"
    $skipped++
    continue
  }
  if (-not $Execute) {
    Write-Host "SAFE TO DELETE: $branch (merged PR #$number, SHA $expected)"
    continue
  }
  # Atomic remote SHA lease prevents deleting a branch changed after validation.
  $ref = "refs/heads/$branch"
  & git -C $Repo push origin "--force-with-lease=${ref}:${expected}" ":$ref"
  if ($LASTEXITCODE -ne 0) {
    Write-Warning "SKIP: push deletion rejected for $branch ($LASTEXITCODE)"
    $skipped++
    continue
  }
  $removed++
  Write-Host "DELETED: $branch"
}
Write-Host "Summary: deleted=$removed skipped=$skipped (main always preserved)"
if ($skipped) { throw "Some branches were skipped; inspect warnings." }
