<#
.SYNOPSIS
  Frozen eval (no training) of a checkpoint, in the random-spawn + two-phase-hunt world.
  Use this to read the "emergence spectrum" of a policy without changing it.
.PARAMETER Ckpt
  Checkpoint to eval, relative to project root. Default runs\ft_bc10.pt (current best).
.PARAMETER Secs
  How long to let episodes accumulate before printing the summary. Default 150.
.PARAMETER Tag
  Output name. Writes runs\<Tag>.jsonl + logs. Default eval_frozen.
.EXAMPLE
  ... -File eval_frozen.ps1 -Ckpt runs\ft_bc10.pt
  ... -File eval_frozen.ps1 -Ckpt runs\r1_cook_s24.pt -Tag eval_champ
.NOTES
  --no-update = parameters frozen. DP_SPAWN_RANDOM + DP_HUNT_V2 are ON.
  Read prey_kills (NOT kills=, which is the reward) for real kill counts.
  Feed runs\<Tag>.jsonl to analyze_spectrum.py for the 23-action / 10-milestone readout.
#>
param(
  [string]$Ckpt = 'runs\ft_bc10.pt',
  [int]$Secs = 150,
  [string]$Tag = 'eval_frozen'
)
$dp  = 'D:\claude-code\c++\routes\deep_protagonist'
$exe = "$dp\build_d122_v2\bin\deep_protagonist_train.exe"
Set-Location $dp
$out = "$dp\runs\$Tag.stdout.log"; $err = "$dp\runs\$Tag.stderr.log"
Remove-Item $out,$err,"$dp\runs\$Tag.jsonl" -ErrorAction SilentlyContinue
$argline = "--policy ppo --load $Ckpt --no-update --weather --allow-cook --n-envs 1 " +
           "--steps 3000000 --seed 24 --metrics runs\$Tag.jsonl"
$cmdline = 'cmd /c "set DP_SPAWN_RANDOM=1&& set DP_HUNT_V2=1&& "' + $exe + '" ' + $argline + ' > "' + $out + '" 2> "' + $err + '""'
$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = $cmdline; CurrentDirectory = $dp }
Write-Output ("EVAL $Ckpt PID=" + $r.ProcessId + " RV=" + $r.ReturnValue + "  (accumulating ${Secs}s...)")
Start-Sleep -Seconds $Secs
$lines = Select-String -Path $out -Pattern 'explore_cells=' | ForEach-Object { $_.Line }
$eps=$lines.Count; $killsum=0; $killeps=0; $reachN=0; $preymin=1e9; $Rsum=0.0; $aliveN=0
foreach ($l in $lines) {
  if ($l -match 'prey_kills=(\d+)')        { $k=[int]$Matches[1]; $killsum+=$k; if($k -gt 0){$killeps++} }
  if ($l -match 'closest_prey=([\d\.]+)m') { $v=[double]$Matches[1]; if($v -lt $preymin){$preymin=$v} }
  if ($l -match 'atk_prey_reach=([1-9]\d*)') { $reachN++ }
  if ($l -match ' R=([+\-][\d\.]+)')       { $Rsum+=[double]$Matches[1] }
  if ($l -match 'steps=9000')              { $aliveN++ }
}
$Ravg = if ($eps -gt 0) { [math]::Round($Rsum/$eps,1) } else { 0 }
Write-Output ("EVAL[$Tag] eps=$eps kills_total=$killsum kill_eps=$killeps reach>0_eps=$reachN prey_min=${preymin}m R_avg=$Ravg full_survive_eps=$aliveN")
Write-Output "jsonl -> runs\$Tag.jsonl  (run analyze_spectrum.py on it)"
