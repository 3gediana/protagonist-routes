# D-122 SYSTEMIC FIX run (e = economy). Resumes the r1_cook_s24 bloodline
# (true resume: matching dims -> no surgery -> loads .opt, params NOT reset) on
# the build_d122_v2 exe that now carries the closed-loop audit's ROOT fixes,
# ALL gated on cook_enabled so the v7 world stays byte-identical (iron law):
#   1. cold-night scarcity: a sheltered night NO LONGER regenerates HUNGER, so
#      "forage + sit in house forever" stops being the free optimum -> stored
#      cooked rations become a real survival need (hunt->cook->stockpile pays).
#   2. convex per-vital DANGER penalty: squared deficit so the single neglected
#      vital dominates the blame -> PPO gradient + GAE back-prop of death pinpoint
#      the decision window that let THAT vital slide (precise credit assignment).
#   (meat-on-hand holding potential from the _h run is retained, not removed.)
# critic-warmup 64 recalibrates the value head to the shifted return landscape
# before policy updates -> guards against the D-056/59 resume-collapse lesson.
$ErrorActionPreference = 'Stop'
$dp  = 'D:\claude-code\c++\routes\deep_protagonist'
$exe = "$dp\build_d122_v2\bin\deep_protagonist_train.exe"
$out = "$dp\runs\r1_cook_s24_e.stdout.log"
$err = "$dp\runs\r1_cook_s24_e.stderr.log"
Remove-Item $out,$err -ErrorAction SilentlyContinue

$argline = '--policy ppo --load runs\r1_cook_s24.pt --weather --allow-cook ' +
           '--ent-coef 0.0015 --critic-warmup 64 --lr 1.5e-4 --ppo-target-kl 0.08 ' +
           '--n-envs 8 --steps 48000000 --save-path runs\r1_cook_s24_e.pt --save-every 25 ' +
           '--metrics runs\r1_cook_s24_e.jsonl --seed 24'
$cmdline = 'cmd /c ""' + $exe + '" ' + $argline + ' > "' + $out + '" 2> "' + $err + '""'
$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{
        CommandLine      = $cmdline
        CurrentDirectory = $dp
     }
Write-Output ("ReturnValue=" + $r.ReturnValue + " (0=ok)")
Write-Output ("PID=" + $r.ProcessId)
