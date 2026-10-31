cd D:\claude-code\c++
Write-Host "=== existing agent_trace.csv on disk ==="
Get-ChildItem -Recurse -Filter agent_trace.csv -ErrorAction SilentlyContinue | ForEach-Object {
  $mb = [math]::Round($_.Length/1MB,1)
  Write-Host ("{0} MB  {1}  {2}" -f $mb, $_.LastWriteTime, $_.FullName)
}
Write-Host "=== s55274 current gen ==="
$last = (Select-String -Path tmp/runlogs/long_run_80gen_stage4b_s55274_tail.log -Pattern 'checkpoint saved' | Select-Object -Last 1).Line
Write-Host $last
