Write-Output '=== Disk ==='
Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='D:'" | Select-Object DeviceID,@{n='FreeGB';e={[math]::Round($_.FreeSpace/1GB,2)}},@{n='SizeGB';e={[math]::Round($_.Size/1GB,2)}} | Format-Table -AutoSize
Write-Output '=== Remaining training processes ==='
$procs=Get-CimInstance Win32_Process | Where-Object { $_.Name -match 'neural-eco|deep_protagonist|cmd' -and $_.CommandLine -match 'onepot_sD|ft_hyp|deep_protagonist_train|neural-eco-protagonist' }
if($procs){$procs | ForEach-Object { "$($_.ProcessId)`t$($_.Name)`t$($_.CommandLine)" }} else { 'none' }
Write-Output '=== Docs ==='
Get-Item 'C:\Users\sans\Desktop\agent\prompt\HANDOFF_2026-06-08_TRAINING_CLEANUP.md','D:\claude-code\c++\docs\experiments\TRAINING_HANDOFF_2026-06-08.md','D:\claude-code\c++\routes\protagonist_ecology\docs\TRAINING_HANDOFF_2026-06-08.md','D:\claude-code\c++\routes\deep_protagonist\docs\TRAINING_HANDOFF_2026-06-08.md' | Select-Object FullName,Length,LastWriteTime | Format-Table -AutoSize
Write-Output '=== Git commits ==='
cd 'D:\claude-code\c++'; git log -4 --oneline
Write-Output '=== Cleanup manifest ==='
Get-ChildItem 'D:\claude-code\c++\CLEANUP_20260608_manifest_*.txt' | Sort-Object LastWriteTime -Descending | Select-Object -First 1 FullName,Length,LastWriteTime | Format-Table -AutoSize
