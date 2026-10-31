cd D:\claude-code\c++
$base = "data/runs/long_run_80gen_stage4b_s55274_tail"
$rd = (Get-ChildItem $base -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
Write-Host "RUNDIR=$rd"
$last = (Select-String -Path tmp/runlogs/long_run_80gen_stage4b_s55274_tail.log -Pattern 'checkpoint saved' | Select-Object -Last 1).Line
Write-Host "LASTGEN: $last"
Write-Host "=== trace generation dirs ==="
Get-ChildItem "$rd/trace" -Directory -ErrorAction SilentlyContinue | ForEach-Object {
  $ep = Get-ChildItem "$($_.FullName)" -Directory -ErrorAction SilentlyContinue
  foreach($e in $ep){
    $csv = Join-Path $e.FullName "agent_trace.csv"
    $sz = if(Test-Path $csv){[math]::Round((Get-Item $csv).Length/1MB,1)}else{"-"}
    Write-Host ("{0}/{1}  agent_trace={2}MB" -f $_.Name, $e.Name, $sz)
  }
}
