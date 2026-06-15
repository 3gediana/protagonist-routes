$rd = 'D:\claude-code\c++\routes\protagonist_ecology\data\runs\onepot_sD_warm_50001\20260603-153712'
Get-ChildItem -Path $rd -Recurse -ErrorAction SilentlyContinue |
  Sort-Object LastWriteTime |
  Select-Object -Last 24 |
  ForEach-Object { $_.LastWriteTime.ToString('HH:mm:ss') + '  ' + ([string]$_.Length).PadLeft(10) + '  ' + $_.FullName.Replace($rd,'') }
Write-Output '--- generation_metrics tail (if exists) ---'
$gm = Join-Path $rd 'generation_metrics.csv'
if (Test-Path $gm) { Get-Content $gm -Tail 6 } else { Write-Output 'no generation_metrics.csv' }
