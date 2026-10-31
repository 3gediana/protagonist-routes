<#
.SYNOPSIS
  DP repertoire breadth PBRS smoke test from bc_gen.pt (the generalist BC base with 10/23 actions alive).
  Goal: see if phi_repertoire can push unlocks past 3.33 and re-activate dead mechanisms.
  2M steps = ~1-2 hours smoke; if it works, extend to 8M.
#>
$ErrorActionPreference = 'Stop'
$dp  = 'D:\claude-code\c++\routes\deep_protagonist'
$exe = "$dp\build_d122_v2\bin\deep_protagonist_train.exe"
Set-Location $dp

$Tag      = 'rep_bcgen'
$Load     = 'runs\bc_gen.pt'
$RepShap  = 1.5
$HuntShap = 0.3
$Ent      = 0.02
$Warmth   = 1.0     # keep warmth to prevent freeze-death regression
$NEnvs    = 4
$Seed     = 24
$Steps    = 2000000  # smoke: 2M steps

$out = "$dp\runs\ft_$Tag.stdout.log"; $err = "$dp\runs\ft_$Tag.stderr.log"
Remove-Item $out,$err -ErrorAction SilentlyContinue

$argline = "--policy ppo --load $Load --weather --allow-cook " +
           "--ent-coef $Ent --critic-warmup 32 --lr 1.5e-4 --ppo-target-kl 0.08 " +
           "--ppo-hunt-shaping $HuntShap --ppo-repertoire-shaping $RepShap --ppo-warmth-shaping $Warmth " +
           "--n-envs $NEnvs --steps $Steps --save-path runs\ft_$Tag.pt --save-every 25 " +
           "--metrics runs\ft_$Tag.jsonl --seed $Seed"

$setpfx = 'set DP_SPAWN_RANDOM=1&& set DP_HUNT_V2=1&& set DP_PREY_PRIORITY=1&& '
$cmdline = 'cmd /c "' + $setpfx + '"' + $exe + '" ' + $argline + ' > "' + $out + '" 2> "' + $err + '""'

$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{
    CommandLine = $cmdline
    CurrentDirectory = $dp
}
Write-Output ("ft_$Tag PID=" + $r.ProcessId + " RV=" + $r.ReturnValue)
Write-Output ("load=$Load rep=$RepShap hunt=$HuntShap ent=$Ent warmth=$Warmth steps=$Steps")
Write-Output "Monitor: check runs\ft_rep_bcgen.jsonl for unlocks progression"
