# D-121 v7: cap action-noise at the champion's healthy level + cut explore-bonus.
# v6 (clamp[-2.3,0.5] + lr/2 + target-kl 0.08) held rock-solid through the 900-ep
# AND 1456-ep zones (where v1/v2/v3 collapsed): r_alive ~10.5, steps ~6500 from
# ep 200 all the way to ~1300. BUT a SLOW decay began ~ep1318 (r_alive 11->7.1,
# steps 6587->4259 by ep1518). Telemetry was otherwise clean (KL ~0.01-0.03,
# entropy NOT exploding) -- the cause was subtler: the small entropy bonus
# (ent 0.005) kept applying upward pressure and pinned log_std at the +0.5
# ceiling (std=1.65) after ~1300 eps; ceiling-level movement noise slowly eroded
# navigation -> shorter episodes -> lower survival. Fix attacks the source:
#   1) clamp ceiling 0.5 -> -0.65 (std 1.65 -> ~0.52, in exe): movement noise can
#      never exceed the champion's proven-healthy level (-0.693), only get quieter
#   2) --ent-coef 0.005 -> 0.0015: minimal upward pressure (still nonzero to avoid
#      premature determinism), so log_std settles near the champion, not the cap
# Kept from v6: --lr 1.5e-4, --ppo-target-kl 0.08, --critic-warmup 64, no shaping.
# Fresh warmstart from s5_v2.pt champion (NOT decayed w1_v6.pt). weather ON, seed 24.
$ErrorActionPreference = 'Stop'
$dp  = 'D:\claude-code\c++\routes\deep_protagonist'
$exe = "$dp\build\bin\deep_protagonist_train.exe"
$out = "$dp\runs\w1_v7.stdout.log"
$err = "$dp\runs\w1_v7.stderr.log"

Remove-Item $out,$err -ErrorAction SilentlyContinue

$argline = '--policy ppo --load runs\s5_v2.pt --weather --ent-coef 0.0015 --critic-warmup 64 ' +
           '--lr 1.5e-4 --ppo-target-kl 0.08 ' +
           '--n-envs 8 --steps 48000000 --save-path runs\w1_v7.pt --save-every 25 ' +
           '--metrics runs\w1_v7.jsonl --seed 24'
$cmdline = 'cmd /c ""' + $exe + '" ' + $argline + ' > "' + $out + '" 2> "' + $err + '""'

$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{
        CommandLine      = $cmdline
        CurrentDirectory = $dp
     }
Write-Output ("ReturnValue=" + $r.ReturnValue + " (0=ok)")
Write-Output ("PID=" + $r.ProcessId)
