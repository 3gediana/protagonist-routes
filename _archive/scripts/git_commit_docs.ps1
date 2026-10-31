cd 'D:\claude-code\c++'
$paths=@(
 'docs/experiments/TRAINING_HANDOFF_2026-06-08.md',
 'routes/protagonist_ecology/docs/TRAINING_HANDOFF_2026-06-08.md',
 'routes/deep_protagonist/docs/TRAINING_HANDOFF_2026-06-08.md',
 'routes/protagonist_ecology/onepot_sD_persist_50023.toml',
 'routes/protagonist_ecology/onepot_sD_persist_ckpt_50023.toml',
 'routes/protagonist_ecology/onepot_sD_shortrun_50022.toml',
 'routes/protagonist_ecology/onepot_sD_shortrun_50023.toml',
 'routes/protagonist_ecology/onepot_sD_shortrun_50024.toml',
 'routes/protagonist_ecology/onepot_sD_explore_50022.toml',
 'routes/protagonist_ecology/onepot_sD_explore_50023.toml',
 'routes/protagonist_ecology/onepot_sD_explore_50024.toml',
 'routes/protagonist_ecology/onepot_sD_huntwide_50022.toml',
 'routes/protagonist_ecology/onepot_sD_huntwide_50023.toml',
 'routes/protagonist_ecology/onepot_sD_huntwide_50024.toml',
 'routes/protagonist_ecology/onepot_sD_hybrid_50022.toml',
 'routes/protagonist_ecology/onepot_sD_hybrid_50023.toml',
 'routes/protagonist_ecology/onepot_sD_hybrid_50024.toml'
)
git add -- $paths
Write-Output '=== staged ==='
git diff --cached --stat
if((git diff --cached --name-only | Measure-Object).Count -gt 0){
  git commit -m 'training: record PE/DP sweep handoff and configs'
}
Write-Output '=== recent log ==='
git log -3 --oneline
