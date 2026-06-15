<#
.SYNOPSIS
  PPO finetune in the random-spawn + two-phase-hunt world, from a survival base.
  This is the D-135 pipeline that grew kills from 0.
.PARAMETER Tag    Output name -> runs\ft_<Tag>.pt + runs\ft_<Tag>.jsonl
.PARAMETER Load   Base checkpoint. Use runs\bc_hv2.pt (BC base -> kills grow) NOT the
                  old champion (runs\r1_cook_s24.pt -> stays 0, old policy can't be moved).
.PARAMETER HuntShap  Hunt-shaping PBRS strength. 0.5 (steady) .. 1.0 (aggressive). Default 0.5.
.PARAMETER Steps  Total env steps. Default 8,000,000 (several hours).
.EXAMPLE
  ... -File train_hunt.ps1 -Tag bc05 -Load runs\bc_hv2.pt -HuntShap 0.5
.NOTES
  Env ON: DP_SPAWN_RANDOM, DP_HUNT_V2, DP_PREY_PRIORITY.
  Detached via Win32_Process Create -> survives SSH disconnect. Checkpoints every 25 updates.
  Watch with monitor.ps1 (prey_kills should climb off 0 for BC-based runs).
#>
param(
  [Parameter(Mandatory=$true)][string]$Tag,
  [string]$Load = 'runs\bc_hv2.pt',
  [double]$HuntShap = 0.5,
  [int]$NEnvs = 4,
  [int]$Seed  = 24,
  [int]$Steps = 8000000
)
$ErrorActionPreference = 'Stop'
$dp  = 'D:\claude-code\c++\routes\deep_protagonist'
$exe = "$dp\build_d122_v2\bin\deep_protagonist_train.exe"
$out = "$dp\runs\ft_$Tag.stdout.log"; $err = "$dp\runs\ft_$Tag.stderr.log"
Remove-Item $out,$err -ErrorAction SilentlyContinue
$argline = "--policy ppo --load $Load --weather --allow-cook " +
           "--ent-coef 0.0015 --critic-warmup 32 --lr 1.5e-4 --ppo-target-kl 0.08 " +
           "--ppo-hunt-shaping $HuntShap " +
           "--n-envs $NEnvs --steps $Steps --save-path runs\ft_$Tag.pt --save-every 25 " +
           "--metrics runs\ft_$Tag.jsonl --seed $Seed"
$setpfx = 'set DP_SPAWN_RANDOM=1&& set DP_HUNT_V2=1&& set DP_PREY_PRIORITY=1&& '
$cmdline = 'cmd /c "' + $setpfx + '"' + $exe + '" ' + $argline + ' > "' + $out + '" 2> "' + $err + '""'
$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = $cmdline; CurrentDirectory = $dp }
Write-Output ("ft_$Tag PID=" + $r.ProcessId + " RV=" + $r.ReturnValue + " load=$Load shap=$HuntShap steps=$Steps")
