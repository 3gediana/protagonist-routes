$ErrorActionPreference = 'Continue'
$r = 'D:/claude-code/c++/routes/deep_protagonist'
Set-Location $r
$exe = "$r/build/bin/deep_protagonist_train.exe"
& $exe --policy ppo --load runs/bc_v9.pt --no-update --deterministic --nutrition --steps 1200 --seed 2100000 *> runs/_surgcheck.log
Write-Output ("EXE_EXIT=" + $LASTEXITCODE)
Write-Output '--- load / surgery / error lines ---'
Select-String -Path runs/_surgcheck.log -Pattern 'surgery|checkpoint loaded|encoder in-dim|obs_dim|terminate|exception|error|Assert|abort' | Select-Object -First 20 | ForEach-Object { $_.Line }
Write-Output '--- last 6 lines of log ---'
Get-Content runs/_surgcheck.log -Tail 6
