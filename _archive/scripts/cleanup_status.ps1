Write-Output '=== Disk ==='
Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='D:'" | Select-Object DeviceID,@{n='FreeGB';e={[math]::Round($_.FreeSpace/1GB,2)}},@{n='SizeGB';e={[math]::Round($_.Size/1GB,2)}} | Format-Table -AutoSize
Write-Output '=== Running training processes ==='
Get-CimInstance Win32_Process | Where-Object { $_.Name -match 'neural-eco|deep_protagonist|cmd' -and $_.CommandLine -match 'onepot_sD|ft_hyp|deep_protagonist|neural-eco' } | ForEach-Object { "$($_.ProcessId)`t$($_.Name)`t$($_.CommandLine)" }
Write-Output '=== Top D:\claude-code\c++ subdirs ==='
Get-ChildItem 'D:\claude-code\c++' -Directory -ErrorAction SilentlyContinue | ForEach-Object { $s=(Get-ChildItem $_.FullName -Recurse -Force -ErrorAction SilentlyContinue | Measure-Object Length -Sum).Sum; [pscustomobject]@{Path=$_.FullName; GB=[math]::Round($s/1GB,2)} } | Sort-Object GB -Descending | Select-Object -First 25 | Format-Table -AutoSize
