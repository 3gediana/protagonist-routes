# D-122 RUNG 1 (cook+light) new bloodline. Warm-start from the v7 champion
# (DISC_CATS=18) -> surgery expands to 23 (cook + 4 future rungs, zero-init) and
# obs 573->582. --allow-cook turns ON the world cook/light economy AND unmasks
# the cook categorical class so the policy can learn it. All v7 hyperparams kept
# (params NOT reset, old reward weights retained -> training iron law).
# Single seed (24, == v7) on the single 8GB GPU for a clean regression compare.
$ErrorActionPreference = 'Stop'
$dp  = 'D:\claude-code\c++\routes\deep_protagonist'
$exe = "$dp\build_d122\bin\deep_protagonist_train.exe"
$out = "$dp\runs\r1_cook_s24.stdout.log"
$err = "$dp\runs\r1_cook_s24.stderr.log"
Remove-Item $out,$err -ErrorAction SilentlyContinue

$argline = '--policy ppo --load runs\w1_v7.pt --weather --allow-cook ' +
           '--ent-coef 0.0015 --critic-warmup 64 --lr 1.5e-4 --ppo-target-kl 0.08 ' +
           '--n-envs 8 --steps 48000000 --save-path runs\r1_cook_s24.pt --save-every 25 ' +
           '--metrics runs\r1_cook_s24.jsonl --seed 24'
$cmdline = 'cmd /c ""' + $exe + '" ' + $argline + ' > "' + $out + '" 2> "' + $err + '""'
$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{
        CommandLine      = $cmdline
        CurrentDirectory = $dp
     }
Write-Output ("ReturnValue=" + $r.ReturnValue + " (0=ok)")
Write-Output ("PID=" + $r.ProcessId)
