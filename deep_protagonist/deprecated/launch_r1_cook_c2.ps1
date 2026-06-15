# D-122 RUNG 1 cook+light — CONTINUATION (c2) of the r1_cook_s24 bloodline.
# The first 48M-step run finished its full budget naturally (not crashed) and
# converged on a forage/craft/build strategy; cook never emerged because the
# agent never hunts (kills=0 -> no RawMeat to cook). This continuation RESUMES
# the SAME bloodline from its own checkpoint: --load runs\r1_cook_s24.pt has
# matching dims (582 obs / 23 cats) so NO surgery runs (PPO.cpp guards) and the
# saved Adam state (r1_cook_s24.pt.opt) is loaded -> true resume with optimizer
# momentum. Params NOT reset, old reward weights retained -> training iron law.
$ErrorActionPreference = 'Stop'
$dp  = 'D:\claude-code\c++\routes\deep_protagonist'
$exe = "$dp\build_d122\bin\deep_protagonist_train.exe"
$out = "$dp\runs\r1_cook_s24_c2.stdout.log"
$err = "$dp\runs\r1_cook_s24_c2.stderr.log"
Remove-Item $out,$err -ErrorAction SilentlyContinue

$argline = '--policy ppo --load runs\r1_cook_s24.pt --weather --allow-cook ' +
           '--ent-coef 0.0015 --critic-warmup 64 --lr 1.5e-4 --ppo-target-kl 0.08 ' +
           '--n-envs 8 --steps 48000000 --save-path runs\r1_cook_s24_c2.pt --save-every 25 ' +
           '--metrics runs\r1_cook_s24_c2.jsonl --seed 24'
$cmdline = 'cmd /c ""' + $exe + '" ' + $argline + ' > "' + $out + '" 2> "' + $err + '""'
$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{
        CommandLine      = $cmdline
        CurrentDirectory = $dp
     }
Write-Output ("ReturnValue=" + $r.ReturnValue + " (0=ok)")
Write-Output ("PID=" + $r.ProcessId)
