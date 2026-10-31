cd D:\claude-code\c++\routes\protagonist_ecology
Write-Host "=== files mentioning main_world_agents_snapshot ==="
Get-ChildItem -Recurse -Include *.cpp,*.h,*.hpp,*.cc -ErrorAction SilentlyContinue | Select-String -Pattern 'main_world_agents_snapshot' -List | ForEach-Object { $_.Path }
Write-Host ""
Write-Host "=== files mentioning snapshot (count per file) ==="
Get-ChildItem -Recurse -Include *.cpp,*.h,*.hpp,*.cc -ErrorAction SilentlyContinue | Select-String -Pattern 'snapshot' -List | ForEach-Object { $_.Path }
Write-Host ""
Write-Host "=== files mentioning load/resume/warm/restore population ==="
Get-ChildItem -Recurse -Include *.cpp,*.h,*.hpp,*.cc -ErrorAction SilentlyContinue | Select-String -Pattern 'loadSnapshot|resume|warm_start|warmStart|restoreFrom|seed_from|seedFrom|initial_population|load_checkpoint|loadCheckpoint' -List | ForEach-Object { $_.Path + '  :: ' + $_.Line.Trim() }
