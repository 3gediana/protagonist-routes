<#
.SYNOPSIS
  D-136 broad-emergence finetune: PPO from the hunting base (ft_bc10.pt) with a
  REPERTOIRE-BREADTH PBRS reward on top of modest hunt shaping. Goal = widen the
  set of mechanisms the policy actually uses, instead of grinding one scalar.
.DESCRIPTION
  phi_repertoire = number of DISTINCT episode milestones reached so far
  (drink/eat/collect/spear/shelter/clothing/seed/house/crop/follower). PBRS gives
  a one-time pulse the FIRST time each new mechanism fires this episode; holding
  pays ~0. Bounded at 10 -> cannot be farmed (anti-Goodhart). All milestone flags
  are gated on GENUINE effects (ate>0, craft succeeded, worn, ...), not button
  presses, so it does not reward "empty presses".
.PARAMETER Tag      Output -> runs\ft_<Tag>.pt + runs\ft_<Tag>.jsonl
.PARAMETER Load     Base ckpt. Default runs\ft_bc10.pt (keeps the learned hunt).
.PARAMETER RepShap  Repertoire-breadth PBRS strength (the new lever). Default 1.0.
.PARAMETER HuntShap Hunt-chain PBRS strength (keep modest so it doesn't dominate). Default 0.3.
.PARAMETER NEnvs    Parallel envs. Default 4.
.PARAMETER Steps    Total env steps. Default 8,000,000.
.NOTES
  Env ON: DP_SPAWN_RANDOM, DP_HUNT_V2, DP_PREY_PRIORITY. Detached (survives SSH).
  Monitor BREADTH (milestones solved / distinct actions used) NOT a single number.
#>
param(
  [Parameter(Mandatory=$true)][string]$Tag,
  [string]$Load = 'runs\ft_bc10.pt',
  [double]$RepShap  = 1.5,
  [double]$HuntShap = 0.3,
  [double]$Ent      = 0.01,
  [double]$Warmth   = 0.0,   # D-136: --ppo-warmth-shaping coef (body-temp recovery PBRS; 0=off)
  [int]$NEnvs = 4,
  [int]$Seed  = 24,
  [int]$Steps = 8000000,
  [switch]$LeanTo   # D-136: set DP_LEANTO=1 (re-enable 1-action lean-to shelter)
)
$ErrorActionPreference = 'Stop'
$dp  = 'D:\claude-code\c++\routes\deep_protagonist'
$exe = "$dp\build_d122_v2\bin\deep_protagonist_train.exe"
$out = "$dp\runs\ft_$Tag.stdout.log"; $err = "$dp\runs\ft_$Tag.stderr.log"
Remove-Item $out,$err -ErrorAction SilentlyContinue
$argline = "--policy ppo --load $Load --weather --allow-cook " +
           "--ent-coef $Ent --critic-warmup 32 --lr 1.5e-4 --ppo-target-kl 0.08 " +
           "--ppo-hunt-shaping $HuntShap --ppo-repertoire-shaping $RepShap --ppo-warmth-shaping $Warmth " +
           "--n-envs $NEnvs --steps $Steps --save-path runs\ft_$Tag.pt --save-every 25 " +
           "--metrics runs\ft_$Tag.jsonl --seed $Seed"
$setpfx = 'set DP_SPAWN_RANDOM=1&& set DP_HUNT_V2=1&& set DP_PREY_PRIORITY=1&& '
if ($LeanTo) { $setpfx += 'set DP_LEANTO=1&& ' }
$cmdline = 'cmd /c "' + $setpfx + '"' + $exe + '" ' + $argline + ' > "' + $out + '" 2> "' + $err + '""'
$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = $cmdline; CurrentDirectory = $dp }
Write-Output ("ft_$Tag PID=" + $r.ProcessId + " RV=" + $r.ReturnValue + " load=$Load rep=$RepShap hunt=$HuntShap ent=$Ent leanto=$LeanTo warmth=$Warmth steps=$Steps")
