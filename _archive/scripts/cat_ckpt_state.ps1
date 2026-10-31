cd D:\claude-code\c++
$ts = (Get-ChildItem 'data/runs/long_run_80gen_stage4b_s63810_tail' -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
Write-Host "RUNTS=$ts"
Write-Host "=== checkpoint_gen79/checkpoint_state.toml ==="
Get-Content (Join-Path $ts 'checkpoint_gen79/checkpoint_state.toml')
Write-Host "=== checkpoint_gen79 top-level entries ==="
Get-ChildItem (Join-Path $ts 'checkpoint_gen79') | Select-Object Name,Mode | Format-Table -AutoSize | Out-String | Write-Host
$pg = (Get-ChildItem (Join-Path $ts 'checkpoint_gen79') -Filter 'genome_*.bin').Count
Write-Host "protagonist genome_*.bin count = $pg"
