$dp = 'D:\claude-code\c++\routes\deep_protagonist'
Set-Location $dp
# D-131 coverage dose-response. All resume the champion, identical params
# except DP_EXPLORE_REW (per-NEW-cell bonus, cell=24m). ja=control (0) must
# stay confined; jb..je should show explore_cells climb + closest_prey drop
# as PPO learns roaming pays. Watch for degeneration into pure wandering.
& "$dp\launch_variant.ps1" -tag "ja_ctrl" -envsets "DP_EXPLORE_REW=0.0"
& "$dp\launch_variant.ps1" -tag "jb_e01"  -envsets "DP_EXPLORE_REW=0.1"
& "$dp\launch_variant.ps1" -tag "jc_e03"  -envsets "DP_EXPLORE_REW=0.3"
& "$dp\launch_variant.ps1" -tag "jd_e06"  -envsets "DP_EXPLORE_REW=0.6"
& "$dp\launch_variant.ps1" -tag "je_e10"  -envsets "DP_EXPLORE_REW=1.0"
Start-Sleep -Seconds 2
Write-Output ("DP running: " + @(Get-Process deep_protagonist_train -ErrorAction SilentlyContinue).Count)
