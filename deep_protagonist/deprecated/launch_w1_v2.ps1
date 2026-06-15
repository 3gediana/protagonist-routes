# D-121 v2: CLEAN weather-world resume. The v1 run added --ppo-hunt-shaping 0.05
# whose potential term exploded (112->854) and dragged the old champion's won
# behaviors down (r_alive 8.2->6.6, r_food .51->.29, r_collect .35->.07) over 929
# eps = catastrophic-forgetting alert + blueprint's "competing potential steals
# budget" (§5b). Fix: DROP shaping. Let the agent adapt to rain purely via vital
# pressure (Goodhart-safe, blueprint intent: mechanisms drive behavior only
# through body-state/survival). Warm-starts s5_v2.pt (571->573 surgery), weather
# ON. WMI-spawned so it survives SSH disconnect.
$ErrorActionPreference = 'Stop'
$dp  = 'D:\claude-code\c++\routes\deep_protagonist'
$exe = "$dp\build\bin\deep_protagonist_train.exe"
$out = "$dp\runs\w1_v2.stdout.log"
$err = "$dp\runs\w1_v2.stderr.log"

Remove-Item $out,$err -ErrorAction SilentlyContinue

$argline = '--policy ppo --load runs\s5_v2.pt --weather ' +
           '--n-envs 8 --steps 48000000 --save-path runs\w1_v2.pt --save-every 25 ' +
           '--metrics runs\w1_v2.jsonl --seed 21'
$cmdline = 'cmd /c ""' + $exe + '" ' + $argline + ' > "' + $out + '" 2> "' + $err + '""'

$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{
        CommandLine      = $cmdline
        CurrentDirectory = $dp
     }
Write-Output ("ReturnValue=" + $r.ReturnValue + " (0=ok)")
Write-Output ("PID=" + $r.ProcessId)
