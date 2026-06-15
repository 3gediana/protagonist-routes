$dp = 'D:\claude-code\c++\routes\deep_protagonist'
Set-Location $dp
# D-132 forage-scarcity dose sweep. NOT a reward: the WORLD is made scarce near
# spawn so leaving the corner becomes instrumentally necessary to keep eating.
# Knobs (env-gated, default off => shipped world unchanged):
#   DP_FORAGE_SCARCITY=1   turn it on
#   DP_FORAGE_BARREN_R     ripe forage kept outside this radius of spawn (m)
#   DP_FORAGE_REDIST_SEC   whole forage field re-scattered this often (s)
# All resume the champion r1_cook_s24.pt, identical params otherwise.
# ka=control (off) must stay confined; kb..ke increase how far the agent must
# travel to eat. Watch explore_cells climb + survival shift camp->migrate.
& "$dp\launch_variant.ps1" -tag "ka_ctrl" -envsets "DP_FORAGE_SCARCITY=0"
& "$dp\launch_variant.ps1" -tag "kb_b60"  -envsets "DP_FORAGE_SCARCITY=1;DP_FORAGE_BARREN_R=60;DP_FORAGE_REDIST_SEC=45"
& "$dp\launch_variant.ps1" -tag "kc_b90"  -envsets "DP_FORAGE_SCARCITY=1;DP_FORAGE_BARREN_R=90;DP_FORAGE_REDIST_SEC=45"
& "$dp\launch_variant.ps1" -tag "kd_b120" -envsets "DP_FORAGE_SCARCITY=1;DP_FORAGE_BARREN_R=120;DP_FORAGE_REDIST_SEC=30"
& "$dp\launch_variant.ps1" -tag "ke_b80s" -envsets "DP_FORAGE_SCARCITY=1;DP_FORAGE_BARREN_R=80;DP_FORAGE_REDIST_SEC=90"
Start-Sleep -Seconds 4
Write-Output ("DP running: " + @(Get-Process deep_protagonist_train -ErrorAction SilentlyContinue).Count)
