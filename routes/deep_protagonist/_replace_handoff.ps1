$ErrorActionPreference = 'Stop'
$desktop = [Environment]::GetFolderPath('Desktop')
Write-Output ("DESKTOP=" + $desktop)
$src = 'D:\claude-code\c++\routes\deep_protagonist\_handoff_new.md'
if (-not (Test-Path $src)) { Write-Output "SRC MISSING: $src"; exit 1 }

$existing = @(Get-ChildItem -Path $desktop -Filter 'DP*.md' -ErrorAction SilentlyContinue)
Write-Output ("found DP*.md count = " + $existing.Count)
foreach ($f in $existing) { Write-Output ("  - " + $f.Name + "  " + $f.Length + "  " + $f.LastWriteTime) }

if ($existing.Count -ge 1) {
  $target = $existing | Sort-Object LastWriteTime -Descending | Select-Object -First 1
  $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
  $bak = Join-Path $desktop ($target.BaseName + '.bak_' + $stamp + '.md')
  Copy-Item -LiteralPath $target.FullName -Destination $bak -Force
  Write-Output ("BACKED UP -> " + $bak)
  Copy-Item -LiteralPath $src -Destination $target.FullName -Force
  $t = Get-Item -LiteralPath $target.FullName
  Write-Output ("REPLACED  -> " + $t.FullName + "  size=" + $t.Length + "  lastWrite=" + $t.LastWriteTime)
} else {
  $dest = Join-Path $desktop 'DP_handoff.md'
  Copy-Item -LiteralPath $src -Destination $dest -Force
  Write-Output ("NO existing found; wrote NEW -> " + $dest)
}
