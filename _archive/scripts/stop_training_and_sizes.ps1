$ErrorActionPreference='Continue'
Write-Output '=== Stopping training processes ==='
$procs = Get-CimInstance Win32_Process | Where-Object { $_.Name -match 'neural-eco|deep_protagonist|cmd' -and $_.CommandLine -match 'onepot_sD|ft_hyp|deep_protagonist_train|neural-eco-protagonist' }
$procs | ForEach-Object { Write-Output "stop $($_.ProcessId) $($_.Name)"; Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
Start-Sleep -Seconds 5
Write-Output '=== Remaining ==='
Get-CimInstance Win32_Process | Where-Object { $_.Name -match 'neural-eco|deep_protagonist|cmd' -and $_.CommandLine -match 'onepot_sD|ft_hyp|deep_protagonist_train|neural-eco-protagonist' } | ForEach-Object { "$($_.ProcessId)`t$($_.Name)`t$($_.CommandLine)" }
Write-Output '=== Disk after stop ==='
Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='D:'" | Select-Object DeviceID,@{n='FreeGB';e={[math]::Round($_.FreeSpace/1GB,2)}},@{n='SizeGB';e={[math]::Round($_.Size/1GB,2)}} | Format-Table -AutoSize
Write-Output '=== Top route dirs ==='
Get-ChildItem 'D:\claude-code\c++\routes' -Directory -ErrorAction SilentlyContinue | ForEach-Object { $s=(Get-ChildItem $_.FullName -Recurse -File -Force -ErrorAction SilentlyContinue | Measure-Object Length -Sum).Sum; [pscustomobject]@{Path=$_.FullName; GB=[math]::Round($s/1GB,2)} } | Sort-Object GB -Descending | Format-Table -AutoSize
Write-Output '=== Top PE run dirs ==='
$peRuns='D:\claude-code\c++\routes\protagonist_ecology\data\runs'
Get-ChildItem $peRuns -Directory -ErrorAction SilentlyContinue | ForEach-Object { $s=(Get-ChildItem $_.FullName -Recurse -File -Force -ErrorAction SilentlyContinue | Measure-Object Length -Sum).Sum; [pscustomobject]@{Path=$_.FullName; GB=[math]::Round($s/1GB,2); Last=$_.LastWriteTime} } | Sort-Object GB -Descending | Select-Object -First 30 | Format-Table -AutoSize
Write-Output '=== Top DP dirs/files ==='
$dp='D:\claude-code\c++\routes\deep_protagonist'
Get-ChildItem $dp -Directory -ErrorAction SilentlyContinue | ForEach-Object { $s=(Get-ChildItem $_.FullName -Recurse -File -Force -ErrorAction SilentlyContinue | Measure-Object Length -Sum).Sum; [pscustomobject]@{Path=$_.FullName; GB=[math]::Round($s/1GB,2)} } | Sort-Object GB -Descending | Select-Object -First 20 | Format-Table -AutoSize
Get-ChildItem "$dp\runs" -File -ErrorAction SilentlyContinue | Sort-Object Length -Descending | Select-Object -First 30 Name,@{n='GB';e={[math]::Round($_.Length/1GB,3)}},Length,LastWriteTime | Format-Table -AutoSize
