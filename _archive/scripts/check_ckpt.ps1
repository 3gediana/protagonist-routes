cd D:\claude-code\c++
Write-Host "=== checkpoint_every / resume_checkpoint_path in s55274 toml ==="
Select-String -Path tmp/layer1/long_run_80gen_stage4b_s55274_tail.toml -Pattern 'checkpoint_every|resume_checkpoint_path|^generations' | ForEach-Object { $_.Line.Trim() }
Write-Host ""
Write-Host "=== checkpoint_gen* dirs across recent runs (name + file count + has checkpoint_state.toml) ==="
Get-ChildItem data/runs -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 6 | ForEach-Object {
  $run = $_.FullName
  $ts = Get-ChildItem $run -Directory -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
  if($ts){
    $cks = Get-ChildItem $ts.FullName -Directory -Filter 'checkpoint_gen*' -ErrorAction SilentlyContinue
    foreach($c in $cks){
      $nfiles = (Get-ChildItem $c.FullName -Recurse -File -ErrorAction SilentlyContinue).Count
      $hasstate = Test-Path (Join-Path $c.FullName 'checkpoint_state.toml')
      Write-Host ("{0} | {1} | files={2} state.toml={3}" -f $_.Name, $c.Name, $nfiles, $hasstate)
    }
  }
}
