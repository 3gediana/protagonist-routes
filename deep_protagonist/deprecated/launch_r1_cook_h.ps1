# D-122 RUNG 1 cook+light — HUNT-NUDGE run (h). Resumes the r1_cook_s24
# bloodline (true resume: matching dims -> no surgery -> loads .opt) but on the
# build_d122_v2 exe that carries the cook-line state-potential nudge: the SAME
# D-043 P2 holding-bonus phi now also pays a small, capped amount for rations on
# hand (raw 0.05/u cap 0.15, cooked 0.18/u cap 0.90), gated on cook_enabled so
# the v7 world stays byte-identical. NOT a per-kill reward (D-064/065 hack-proof
# lesson). Goal: give the agent a gradient toward hunt->cook->stockpile so cook
# can finally emerge. Params NOT reset, no old reward removed -> iron law.
$ErrorActionPreference = 'Stop'
$dp  = 'D:\claude-code\c++\routes\deep_protagonist'
$exe = "$dp\build_d122_v2\bin\deep_protagonist_train.exe"
$out = "$dp\runs\r1_cook_s24_h.stdout.log"
$err = "$dp\runs\r1_cook_s24_h.stderr.log"
Remove-Item $out,$err -ErrorAction SilentlyContinue

$argline = '--policy ppo --load runs\r1_cook_s24.pt --weather --allow-cook ' +
           '--ent-coef 0.0015 --critic-warmup 64 --lr 1.5e-4 --ppo-target-kl 0.08 ' +
           '--n-envs 8 --steps 48000000 --save-path runs\r1_cook_s24_h.pt --save-every 25 ' +
           '--metrics runs\r1_cook_s24_h.jsonl --seed 24'
$cmdline = 'cmd /c ""' + $exe + '" ' + $argline + ' > "' + $out + '" 2> "' + $err + '""'
$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{
        CommandLine      = $cmdline
        CurrentDirectory = $dp
     }
Write-Output ("ReturnValue=" + $r.ReturnValue + " (0=ok)")
Write-Output ("PID=" + $r.ProcessId)
