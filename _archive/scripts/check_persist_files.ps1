$dir='D:\claude-code\c++\routes\protagonist_ecology\data\runs\onepot_sD_warm_50023\20260607-201141'
Write-Output "dir exists: $(Test-Path $dir)"
if (Test-Path $dir) {
  Get-ChildItem $dir | Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
  Write-Output 'checkpoints:'
  Get-ChildItem $dir -Directory -Filter 'checkpoint_gen*' -ErrorAction SilentlyContinue | Select-Object Name,LastWriteTime | Format-Table -AutoSize
}
