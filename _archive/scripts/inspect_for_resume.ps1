cd D:\claude-code\c++
Write-Host "=== s55274 progress ==="
Test-Path tmp/runlogs/long_run_80gen_stage4b_s55274_tail.done
(Select-String -Path tmp/runlogs/long_run_80gen_stage4b_s55274_tail.log -Pattern 'checkpoint saved' | Select-Object -Last 1).Line
Write-Host ""
Write-Host "=== toml: [runtime] section + generations + checkpoint_every + resume ==="
$t = Get-Content tmp/layer1/long_run_80gen_stage4b_s63810_tail.toml
for($i=0;$i -lt $t.Count;$i++){
  if($t[$i] -match 'resume_checkpoint_path|checkpoint_every|^\s*generations\s*=|^\[runtime\]|^\[evolution\]'){
    Write-Host ("{0}: {1}" -f ($i+1), $t[$i])
  }
}
Write-Host ""
Write-Host "=== s63810 checkpoint_gen79 background species dirs ==="
$ts = (Get-ChildItem 'data/runs/long_run_80gen_stage4b_s63810_tail' -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
Get-ChildItem (Join-Path $ts 'checkpoint_gen79') -Directory | ForEach-Object { $n=(Get-ChildItem $_.FullName -File).Count; Write-Host ("{0}  files={1}" -f $_.Name,$n) }
