# D-121 v4: same stabilized weather-warmstart config as v3, but built with the
# DEATH-CAUSE TELEMETRY FIX. v1/v2/v3 all degraded (r_alive slid, episodes
# shrank) and I was diagnosing blind because the VecEnv metrics path logged the
# IDLE TEMPLATE env's vitals (deaths=0, protein/vitamin=100 always) instead of
# the env(i) that finished the episode. Fixed log_episode to read venv.env(i).
# Now deaths/deaths_cold/food/protein/vitamin + protein/vitamin + bldg/night/
# explored reflect the REAL finishing env -> we can see WHY episodes end.
# Config unchanged: s5_v2.pt warmstart (571->573 surgery), weather ON,
# --ent-coef 0.005 + --critic-warmup 64, NO behavior shaping. seed 24.
$ErrorActionPreference = 'Stop'
$dp  = 'D:\claude-code\c++\routes\deep_protagonist'
$exe = "$dp\build\bin\deep_protagonist_train.exe"
$out = "$dp\runs\w1_v4.stdout.log"
$err = "$dp\runs\w1_v4.stderr.log"

Remove-Item $out,$err -ErrorAction SilentlyContinue

$argline = '--policy ppo --load runs\s5_v2.pt --weather --ent-coef 0.005 --critic-warmup 64 ' +
           '--n-envs 8 --steps 48000000 --save-path runs\w1_v4.pt --save-every 25 ' +
           '--metrics runs\w1_v4.jsonl --seed 24'
$cmdline = 'cmd /c ""' + $exe + '" ' + $argline + ' > "' + $out + '" 2> "' + $err + '""'

$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{
        CommandLine      = $cmdline
        CurrentDirectory = $dp
     }
Write-Output ("ReturnValue=" + $r.ReturnValue + " (0=ok)")
Write-Output ("PID=" + $r.ProcessId)
