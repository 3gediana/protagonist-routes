$runs = Get-ChildItem 'D:\claude-code\c++\routes\protagonist_ecology\data\runs' -Directory | Where-Object { $_.Name -match '50024' }
foreach ($r in $runs) {
  Write-Output $r.FullName
  Get-ChildItem $r.FullName -Directory -Filter 'checkpoint_gen*' | Sort-Object Name | Select-Object -Last 5 | ForEach-Object { Write-Output ("  " + $_.Name) }
}
