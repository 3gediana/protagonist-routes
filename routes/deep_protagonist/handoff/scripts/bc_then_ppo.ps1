<#
.SYNOPSIS
  Full D-135 pipeline orchestration helper: record oracle demos -> BC pretrain -> PPO finetune.
  Runs ONE stage at a time (each stage is long); pass -Stage to pick.
.PARAMETER Stage
  record | bc | ppo   (run in this order; each must finish before the next)
    record : ScriptedSettler oracle plays the hunt world, saves (obs,action) demos.
    bc     : behavioral-cloning pretrain from demos -> runs\bc_<Tag>.pt (survives, 0 kills).
    ppo    : PPO finetune from the BC base (calls train_hunt.ps1) -> kills grow off 0.
.PARAMETER Tag   Name suffix. Default hv2.
.EXAMPLE
  ... -File bc_then_ppo.ps1 -Stage record
  ... -File bc_then_ppo.ps1 -Stage bc
  ... -File bc_then_ppo.ps1 -Stage ppo -Tag bc05
.NOTES
  Demos already exist at runs\demos_hv2_s24.bin (2GB, 88 kills) -- you can skip 'record'.
  BC base already exists at runs\bc_hv2.pt -- you can skip 'bc' and go straight to ppo.
#>
param(
  [Parameter(Mandatory=$true)][ValidateSet('record','bc','ppo')][string]$Stage,
  [string]$Tag = 'hv2'
)
$ErrorActionPreference = 'Stop'
$dp  = 'D:\claude-code\c++\routes\deep_protagonist'
$exe = "$dp\build_d122_v2\bin\deep_protagonist_train.exe"
Set-Location $dp

if ($Stage -eq 'record') {
  $out = "$dp\runs\rec_$Tag.stdout.log"; $err = "$dp\runs\rec_$Tag.stderr.log"
  Remove-Item $out,$err -ErrorAction SilentlyContinue
  $argline = "--policy settler --weather --allow-cook --n-envs 1 --steps 2000000 --seed 24 " +
             "--record-demos runs\demos_$Tag`_s24.bin --metrics runs\rec_$Tag.jsonl"
  $setpfx = 'set DP_SPAWN_RANDOM=1&& set DP_ORACLE_HUNT=1&& set DP_PREY_PRIORITY=1&& set DP_HUNT_V2=1&& '
  $cmdline = 'cmd /c "' + $setpfx + '"' + $exe + '" ' + $argline + ' > "' + $out + '" 2> "' + $err + '""'
  $r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine=$cmdline; CurrentDirectory=$dp }
  Write-Output ("RECORD PID=" + $r.ProcessId + " -> runs\demos_$Tag`_s24.bin (watch rec_$Tag.stdout.log for kill count)")
}
elseif ($Stage -eq 'bc') {
  $olog = "$dp\runs\bc_$Tag.stdout.log"; $elog = "$dp\runs\bc_$Tag.stderr.log"
  Remove-Item $olog,$elog -ErrorAction SilentlyContinue
  $argline = "--policy bc --bc-demos runs\demos_$Tag`_s24.bin --bc-out runs\bc_$Tag.pt " +
             "--bc-epochs 10 --bc-batch 16 --bc-bptt 64 --bc-lr 1e-3 " +
             "--bc-trigger-weight 5 --bc-min-eplen 300 --bc-trigger-oversample 3"
  $cmdline = 'cmd /c ""' + $exe + '" ' + $argline + ' > "' + $olog + '" 2> "' + $elog + '""'
  $r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine=$cmdline; CurrentDirectory=$dp }
  Write-Output ("BC PID=" + $r.ProcessId + " -> runs\bc_$Tag.pt  (watch disc_ce converge ~0.2)")
}
elseif ($Stage -eq 'ppo') {
  Write-Output "Delegating to train_hunt.ps1 (base = runs\bc_$Tag.pt)..."
  & "$PSScriptRoot\train_hunt.ps1" -Tag $Tag -Load "runs\bc_$Tag.pt" -HuntShap 0.5
}
