$pe='D:\claude-code\c++\routes\protagonist_ecology'
foreach($f in Get-ChildItem $pe -File -Filter 'onepot_sD_*.log' | Where-Object {$_.Name -match 'shortrun|explore|huntwide|hybrid'}){
  Write-Output "--- $($f.Name) ---"
  Select-String -Path $f.FullName -Pattern 'data\\runs|data/runs|checkpoint|Run|output|scenario' | Select-Object -First 8 | ForEach-Object {$_.Line}
}
Write-Output '=== kept recent checkpoints after cleanup ==='
Get-ChildItem 'D:\claude-code\c++\routes\protagonist_ecology\data\runs' -Recurse -Directory -Filter 'checkpoint_gen*' -ErrorAction SilentlyContinue | Where-Object {$_.LastWriteTime -gt (Get-Date '2026-06-07T18:00:00')} | Sort-Object LastWriteTime | Select-Object FullName,LastWriteTime | Format-Table -AutoSize
