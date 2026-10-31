$ErrorActionPreference='Stop'
Set-Location 'D:\claude-code\c++'
git status --short
$msg = @"
protagonist_ecology: lifetime MAP-Elites archive + CVT + hunt BD axis

Root cause of coverage oscillation (8->4 then crash):
- Per-generation archive rebuilt each gen, losing cell elites
- Hunt descriptor disabled (hunt_bin_size=0)
- High mutation noise (mutation_boost=2.0, line_sigma=0.15)

Changes:
- QualityDiversityArchive: add lifetime_archive_enabled mode that
  stores genome copies (not just IDs) across generations. Old cell
  elites persist and compete with new submissions via fitness.
- ProtagonistEcologyConfig: add me_neat_lifetime_archive_enabled
  config option (default false for backward compat)
- ProtagonistEvolution: reproduceFromArchive now accepts archive
  genome copies directly (not just IDs from current population)
- ProtagonistEcologyRuntime: hoist archive above generation loop,
  conditionally skip clear() when lifetime mode active

Smoke test (50023, 8 gens from gen329):
  me_cov: 8 -> 26/64 (strictly non-decreasing)
  QD score: 4440 -> 30238
  Active mechanisms: chop/craft/hunt/build/throw all present
  All 8 per-gen checkpoints saved

Config: hunt_bin_size=1, hunt_max_bins=3, cvt_enabled=true,
  cvt_num_centroids=64, lifetime_archive_enabled=true,
  line_sigma=0.05, mutation_boost=1.0
"@
git add routes/protagonist_ecology/runtime/QualityDiversityArchive.h
git add routes/protagonist_ecology/runtime/QualityDiversityArchive.cpp
git add routes/protagonist_ecology/runtime/ProtagonistEvolution.h
git add routes/protagonist_ecology/runtime/ProtagonistEvolution.cpp
git add routes/protagonist_ecology/runtime/ProtagonistEcologyConfig.h
git add routes/protagonist_ecology/runtime/ProtagonistEcologyConfig.cpp
git add routes/protagonist_ecology/runtime/ProtagonistEcologyRuntime.cpp
git commit -m $msg
git log --oneline -3
