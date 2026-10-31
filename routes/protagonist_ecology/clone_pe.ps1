param([int]$seed)
$ErrorActionPreference = 'Stop'
$pe   = 'D:\claude-code\c++\routes\protagonist_ecology'
$base = "$pe\onepot_sD_warm_50002.toml"
$dst  = "$pe\onepot_sD_warm_$seed.toml"
$c = Get-Content $base -Raw
# NOTE: the original shared sC_50002 gen79 warm-start base was deleted during
# disk cleanup. We now warm-start from the preserved sD_warm_50002 gen159
# checkpoint and extend `generations` past the 160 ceiling, so each new
# random_seed explores an independent continuation into uncharted (gen160+)
# evolutionary territory.
$c = $c -replace 'random_seed\s*=\s*50002', "random_seed = $seed"
$c = $c -replace 'resume_checkpoint_path\s*=\s*"[^"]*"', 'resume_checkpoint_path = "D:/claude-code/c++/routes/protagonist_ecology/data/runs/onepot_sD_warm_50002/20260604-115936/checkpoint_gen159"'
$c = $c -replace '(?m)^generations\s*=\s*160', 'generations = 320'
$c = $c -replace 'scenario_name\s*=\s*"onepot_sD_warm_160gen_50002"', "scenario_name = `"onepot_sD_warm_160gen_$seed`""
$c = $c -replace 'runs_dir\s*=\s*"data/runs/onepot_sD_warm_50002"', "runs_dir = `"data/runs/onepot_sD_warm_$seed`""
Set-Content -Path $dst -Value $c -NoNewline
Write-Output "wrote $dst"
# launch
$exe = 'D:\claude-code\c++\build_ninja_cuda\bin\neural-eco-protagonist-batched.exe'
$log = "$pe\onepot_sD_warm_$seed.log"
$cmdline = 'cmd /c "set PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%&& "' + $exe + '" --config onepot_sD_warm_' + $seed + '.toml > "' + $log + '" 2>&1"'
$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine=$cmdline; CurrentDirectory=$pe }
Write-Output ("seed=$seed ReturnValue=" + $r.ReturnValue + " PID=" + $r.ProcessId)
