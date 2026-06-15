# D-121 v6: clamp + two drift-brakes for a stable long unattended run.
# v5 (clamp only) FIXED the catastrophic failure: entropy stayed ~7 (vs v4's 72),
# eat/drink restored, and it held r_alive ~10.7 / steps ~6500 ALL THE WAY through
# the ~900-ep zone where v1/v2/v3 all collapsed (892->963 eps rock solid). BUT a
# slower decay began ~1050-1100 eps: r_alive 10.7->5.3, steps 6440->3193 over
# eps 963->1189, while entropy stayed clamped (~6.8). Cause = policy slowly
# drifting out of the champion basin via accumulated large updates (16 KL spikes
# to 0.46-4.0). Death mix: food/hunger-thirst ~60%, cold ~30% (protein/vitamin
# fine). The agent was surviving 6000+ steps under the SAME weather earlier, so
# weather isn't newly harder -- it's optimizer drift. Brakes:
#   1) --lr 1.5e-4  (half of D-099 default 3e-4; D-101 "finetune -> lower lr")
#   2) --ppo-target-kl 0.08 (decision_round_17 k3 early-stop; loose enough to
#      pass normal ~0.03 updates, cuts off the runaway jumps that drive drift)
# clamp log_std[-2.3,0.5] stays (in exe). Fresh warmstart s5_v2.pt (not the
# decayed w1_v5.pt). weather ON, ent 0.005, critic-warmup 64, no shaping, seed 24.
$ErrorActionPreference = 'Stop'
$dp  = 'D:\claude-code\c++\routes\deep_protagonist'
$exe = "$dp\build\bin\deep_protagonist_train.exe"
$out = "$dp\runs\w1_v6.stdout.log"
$err = "$dp\runs\w1_v6.stderr.log"

Remove-Item $out,$err -ErrorAction SilentlyContinue

$argline = '--policy ppo --load runs\s5_v2.pt --weather --ent-coef 0.005 --critic-warmup 64 ' +
           '--lr 1.5e-4 --ppo-target-kl 0.08 ' +
           '--n-envs 8 --steps 48000000 --save-path runs\w1_v6.pt --save-every 25 ' +
           '--metrics runs\w1_v6.jsonl --seed 24'
$cmdline = 'cmd /c ""' + $exe + '" ' + $argline + ' > "' + $out + '" 2> "' + $err + '""'

$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{
        CommandLine      = $cmdline
        CurrentDirectory = $dp
     }
Write-Output ("ReturnValue=" + $r.ReturnValue + " (0=ok)")
Write-Output ("PID=" + $r.ProcessId)
