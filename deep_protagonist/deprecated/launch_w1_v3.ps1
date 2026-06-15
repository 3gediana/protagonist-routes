# D-121 v3: STABILIZED weather-world warmstart finetune.
# v1/v2 both collapsed at ~900 eps: episodes 8568->694 steps, policy collapsed to
# spamming one action, entropy ballooned (~74). Root cause = default ent_coef 0.01
# is tuned for FRESH init; on a strong-ckpt warmstart it over-rewards entropy,
# inflates log_std and erodes cloned behavior (this trainer's own D-101 note +
# D-068 决策: "load strong ckpt + high ent_coef => entropy climbs, 丢掉已学行为").
# Fix (project-proven warmstart config): --ent-coef 0.005 (D-068 strong-ckpt
# calibration) + --critic-warmup 64 (D-101 BC->PPO: value-only updates first so
# the value head adapts to the weather world before the policy is perturbed).
# Still: s5_v2.pt warmstart (571->573 surgery), weather ON, NO behavior shaping.
$ErrorActionPreference = 'Stop'
$dp  = 'D:\claude-code\c++\routes\deep_protagonist'
$exe = "$dp\build\bin\deep_protagonist_train.exe"
$out = "$dp\runs\w1_v3.stdout.log"
$err = "$dp\runs\w1_v3.stderr.log"

Remove-Item $out,$err -ErrorAction SilentlyContinue

$argline = '--policy ppo --load runs\s5_v2.pt --weather --ent-coef 0.005 --critic-warmup 64 ' +
           '--n-envs 8 --steps 48000000 --save-path runs\w1_v3.pt --save-every 25 ' +
           '--metrics runs\w1_v3.jsonl --seed 21'
$cmdline = 'cmd /c ""' + $exe + '" ' + $argline + ' > "' + $out + '" 2> "' + $err + '""'

$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{
        CommandLine      = $cmdline
        CurrentDirectory = $dp
     }
Write-Output ("ReturnValue=" + $r.ReturnValue + " (0=ok)")
Write-Output ("PID=" + $r.ProcessId)
