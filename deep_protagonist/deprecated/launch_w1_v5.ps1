# D-121 v5: THE FIX. Identical config to v4 (same seed 24) so this is a clean
# controlled test: the ONLY difference vs v4 is the new exe clamps the
# continuous log_std to [-2.3, 0.5] after every optimizer step.
# v4 telemetry (now correct) proved the collapse mechanism: entropy ran to ~72
# (continuous std ~1e9) -> movement/navigation became pure noise -> the agent
# stopped eating/drinking (r_food=r_water=0) -> died of hunger/thirst
# (deaths_food 7-14/env) within a few thousand steps, while the bounded
# discrete head still fired craft/milestone. The unbounded log_std was the true
# root cause of every v1/v2/v3 weather warm-start collapse. With the clamp,
# entropy can no longer explode and the champion's eat/drink/navigate survive.
# Still: s5_v2.pt warmstart (571->573 surgery), weather ON, --ent-coef 0.005
# + --critic-warmup 64, NO behavior shaping. Death-cause telemetry is correct.
$ErrorActionPreference = 'Stop'
$dp  = 'D:\claude-code\c++\routes\deep_protagonist'
$exe = "$dp\build\bin\deep_protagonist_train.exe"
$out = "$dp\runs\w1_v5.stdout.log"
$err = "$dp\runs\w1_v5.stderr.log"

Remove-Item $out,$err -ErrorAction SilentlyContinue

$argline = '--policy ppo --load runs\s5_v2.pt --weather --ent-coef 0.005 --critic-warmup 64 ' +
           '--n-envs 8 --steps 48000000 --save-path runs\w1_v5.pt --save-every 25 ' +
           '--metrics runs\w1_v5.jsonl --seed 24'
$cmdline = 'cmd /c ""' + $exe + '" ' + $argline + ' > "' + $out + '" 2> "' + $err + '""'

$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{
        CommandLine      = $cmdline
        CurrentDirectory = $dp
     }
Write-Output ("ReturnValue=" + $r.ReturnValue + " (0=ok)")
Write-Output ("PID=" + $r.ProcessId)
