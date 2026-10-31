cd 'D:\claude-code\c++'
Write-Output '=== Relevant git status ==='
git status --short -- docs/experiments/TRAINING_HANDOFF_2026-06-08.md routes/protagonist_ecology/docs/TRAINING_HANDOFF_2026-06-08.md routes/deep_protagonist/docs/TRAINING_HANDOFF_2026-06-08.md src_pe/runtime/QualityDiversityArchive.h src_pe/runtime/QualityDiversityArchive.cpp src_pe/runtime/ProtagonistEvolution.h src_pe/runtime/ProtagonistEvolution.cpp src_pe/runtime/ProtagonistEcologyConfig.h src_pe/runtime/ProtagonistEcologyConfig.cpp src_pe/runtime/ProtagonistEcologyRuntime.cpp routes/protagonist_ecology/onepot_sD_*.toml
Write-Output '=== Branch ==='
git branch --show-current
Write-Output '=== Recent log ==='
git log -3 --oneline
