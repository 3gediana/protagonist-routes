$dp = 'D:\claude-code\c++\routes\deep_protagonist'
Set-Location $dp
# D-133 random-spawn sweep. ROOT-CAUSE fix, not a reward: the fixed spawn corner
# is WHY the agent could camp -- every life began in the same water+food corner,
# so "guard this corner" was a complete survival strategy. DP_SPAWN_RANDOM picks
# a fresh valid spawn (water in vision + dry land) anywhere on the map each
# episode, so there is no fixed safe corner. The policy is forced to learn
# survival/foraging wherever it lands AND will sometimes spawn near prey herds,
# finally getting hunting experience it never had.
#   knobs: DP_SPAWN_RANDOM=1  + (optional) the D-132 scarcity knobs on top.
# Judge by closest_prey (should drop / spread low as spawns land near prey),
# atk_prey_reach / opp_reach (start firing), and kills -- NOT explore_cells
# (that is per-episode; range now opens ACROSS episodes via varied spawns).
& "$dp\launch_variant.ps1" -tag "la_ctrl" -envsets "DP_SPAWN_RANDOM=0" -seed 24
& "$dp\launch_variant.ps1" -tag "lb_rand" -envsets "DP_SPAWN_RANDOM=1" -seed 24
& "$dp\launch_variant.ps1" -tag "lc_rand" -envsets "DP_SPAWN_RANDOM=1" -seed 77
& "$dp\launch_variant.ps1" -tag "ld_rsc"  -envsets "DP_SPAWN_RANDOM=1;DP_FORAGE_SCARCITY=1;DP_FORAGE_BARREN_R=90;DP_FORAGE_REDIST_SEC=60" -seed 24
& "$dp\launch_variant.ps1" -tag "le_rsc"  -envsets "DP_SPAWN_RANDOM=1;DP_FORAGE_SCARCITY=1;DP_FORAGE_BARREN_R=120;DP_FORAGE_REDIST_SEC=45" -seed 24
Start-Sleep -Seconds 4
Write-Output ("DP running: " + @(Get-Process deep_protagonist_train -ErrorAction SilentlyContinue).Count)
