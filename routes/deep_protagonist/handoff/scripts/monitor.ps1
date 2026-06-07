<#
.SYNOPSIS
  Show GPU usage, running DP processes, and the recent kill trend of every ft_*.jsonl run.
.PARAMETER Recent  How many most-recent episodes to summarize per run. Default 40.
.EXAMPLE
  ... -File monitor.ps1
.NOTES
  prey_kills = real kills (kills= is the reward, ignore it). For BC-based finetunes you
  want prey_kills climbing off 0; old-champion-based runs stay flat at 0.
#>
param([int]$Recent = 40)
$r = 'D:\claude-code\c++\routes\deep_protagonist\runs'
Write-Output '=== GPU ==='
& 'nvidia-smi' --query-gpu=memory.used,memory.total,utilization.gpu --format=csv,noheader 2>$null
Write-Output ("DP_procs=" + @(Get-Process deep_protagonist_train -EA SilentlyContinue).Count +
              "  PE_procs=" + @(Get-Process neural-eco-protagonist-batched -EA SilentlyContinue).Count)
Write-Output ''
Get-ChildItem "$r\ft_*.jsonl" -EA SilentlyContinue | ForEach-Object {
  $lines = Get-Content $_.FullName -EA SilentlyContinue
  if (-not $lines) { return }
  $tail = $lines | Select-Object -Last $Recent
  $ks=0; $ke=0; $reach=0
  foreach ($l in $tail) {
    if ($l -match '"prey_kills":\s*(\d+)') { $k=[int]$Matches[1]; $ks+=$k; if($k -gt 0){$ke++} }
    elseif ($l -match 'prey_kills=(\d+)')  { $k=[int]$Matches[1]; $ks+=$k; if($k -gt 0){$ke++} }
    if ($l -match 'atk_prey_reach[=":]+\s*([1-9]\d*)') { $reach++ }
  }
  Write-Output ("{0,-28} eps_total={1,5}  last{2}: kills={3} kill_eps={4} reach_eps={5}  updated={6}" -f `
    $_.Name, $lines.Count, $Recent, $ks, $ke, $reach, $_.LastWriteTime.ToString('MM-dd HH:mm'))
}
