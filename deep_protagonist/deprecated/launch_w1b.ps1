# BRIDGE continuation (fire ONLY after D-121/w1_v1 finishes, single 8GB GPU).
# Keeps the backend in valid training (mechanism-complete weather world, unified
# 573-dim weights) with zero GPU idle while W2 code+smoke is prepared offline.
# Warm-starts the FINAL w1_v1.pt (same 573 width -> clean resume, no surgery),
# new seed for fresh trajectories. Swap to D-122-W2 by killing this + launching
# the W2 run once its smoke passes. WMI-spawned so it survives SSH disconnect.
$ErrorActionPreference = 'Stop'
$dp  = 'D:\claude-code\c++\routes\deep_protagonist'
$exe = "$dp\build\bin\deep_protagonist_train.exe"
$out = "$dp\runs\w1_v3b.stdout.log"
$err = "$dp\runs\w1_v3b.stderr.log"

if (-not (Test-Path "$dp\runs\w1_v3.pt")) { Write-Output 'ABORT: runs\w1_v3.pt not found (let D-121 v3 finish first)'; exit 1 }
Remove-Item $out,$err -ErrorAction SilentlyContinue

$argline = '--policy ppo --load runs\w1_v3.pt --weather --ent-coef 0.005 --critic-warmup 64 ' +
           '--n-envs 8 --steps 48000000 --save-path runs\w1_v3b.pt --save-every 25 ' +
           '--metrics runs\w1_v3b.jsonl --seed 22'
$cmdline = 'cmd /c ""' + $exe + '" ' + $argline + ' > "' + $out + '" 2> "' + $err + '""'

$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{
        CommandLine      = $cmdline
        CurrentDirectory = $dp
     }
Write-Output ("ReturnValue=" + $r.ReturnValue + " (0=ok)")
Write-Output ("PID=" + $r.ProcessId)
