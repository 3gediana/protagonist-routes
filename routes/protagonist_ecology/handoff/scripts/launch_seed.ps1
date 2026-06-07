<#
.SYNOPSIS
  Warm-start a new PE evolution seed from the preserved gen159 checkpoint and launch it.
.PARAMETER Seed   Unique random seed / run id. Default 50031.
.PARAMETER Gens   generations ceiling (must exceed the warm checkpoint's gen). Default 320.
.EXAMPLE
  ... -File launch_seed.ps1 -Seed 50031
.NOTES
  Requires CUDA on PATH (handled below). Detached via Win32_Process -> survives SSH drop.
  Warm base = onepot_sD_warm_50002 / checkpoint_gen159 (see PITFALLS: don't delete it).
#>
param([int]$Seed = 50031, [int]$Gens = 320)
$ErrorActionPreference = 'Stop'
$pe   = 'D:\claude-code\c++\routes\protagonist_ecology'
$base = "$pe\onepot_sD_warm_50002.toml"
$dst  = "$pe\onepot_sD_warm_$Seed.toml"
$warm = "D:/claude-code/c++/routes/protagonist_ecology/data/runs/onepot_sD_warm_50002/20260604-115936/checkpoint_gen159"
$c = Get-Content $base -Raw
$c = $c -replace 'random_seed\s*=\s*\d+', "random_seed = $Seed"
$c = $c -replace 'resume_checkpoint_path\s*=\s*"[^"]*"', ('resume_checkpoint_path = "' + $warm + '"')
$c = $c -replace '(?m)^generations\s*=\s*\d+', "generations = $Gens"
$c = $c -replace 'scenario_name\s*=\s*"[^"]*"', "scenario_name = `"onepot_sD_warm_160gen_$Seed`""
$c = $c -replace 'runs_dir\s*=\s*"[^"]*"', "runs_dir = `"data/runs/onepot_sD_warm_$Seed`""
Set-Content -Path $dst -Value $c -NoNewline
$exe = 'D:\claude-code\c++\build_ninja_cuda\bin\neural-eco-protagonist-batched.exe'
$log = "$pe\onepot_sD_warm_$Seed.log"
$cmdline = 'cmd /c "set PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%&& "' + $exe + '" --config onepot_sD_warm_' + $Seed + '.toml > "' + $log + '" 2>&1"'
$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine=$cmdline; CurrentDirectory=$pe }
Write-Output ("seed=$Seed gens=$Gens PID=" + $r.ProcessId + " RV=" + $r.ReturnValue + " log=$log")
