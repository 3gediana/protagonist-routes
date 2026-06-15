# D-121: launch the first Stage-W1 (weather) DP run via WMI Win32_Process.Create
# so the trainer is parented to the WMI host (NOT the SSH session) and keeps
# running after this SSH channel closes. Warm-starts s5_v2.pt (571->573 surgery),
# weather ON (rain douses fire / wet->cold / rain->water) + non-breaking
# phi_hunt_progress PBRS. stdout/stderr redirected to log files via cmd /c.
$ErrorActionPreference = 'Stop'
$dp  = 'D:\claude-code\c++\routes\deep_protagonist'
$exe = "$dp\build\bin\deep_protagonist_train.exe"
$out = "$dp\runs\w1_v1.stdout.log"
$err = "$dp\runs\w1_v1.stderr.log"

# clean any stale probe / prior-attempt artifacts so disk stays tidy
Remove-Item "$dp\runs\w1_probe.pt","$dp\runs\w1_probe.pt.opt","$dp\runs\w1_probe.jsonl" -ErrorAction SilentlyContinue
Remove-Item $out,$err -ErrorAction SilentlyContinue

$argline = '--policy ppo --load runs\s5_v2.pt --weather --ppo-hunt-shaping 0.05 ' +
           '--n-envs 8 --steps 48000000 --save-path runs\w1_v1.pt --save-every 25 ' +
           '--metrics runs\w1_v1.jsonl --seed 21'
$cmdline = 'cmd /c ""' + $exe + '" ' + $argline + ' > "' + $out + '" 2> "' + $err + '""'

$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{
        CommandLine      = $cmdline
        CurrentDirectory = $dp
     }
Write-Output ("ReturnValue=" + $r.ReturnValue + " (0=ok)")
Write-Output ("PID=" + $r.ProcessId)
