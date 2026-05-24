@echo off
REM L6 R3 stage 3 commits A-K + Stone bootstrap commits 1/2/3 end-to-end
REM 60-gen graduate AB. Enables EVERYTHING:
REM   - curriculum.archetype_progression_persistence (Stone 1/2/3)
REM   - triple_world.enabled (three-world topology)
REM   - borderland_eval_period_gens=1 (mini-tick eval every gen)
REM   - borderland_mutate_sigma=0.01 + rates 0.8/0.05 (commit J/K)
REM   - borderland_min_continuity_gens=5 (commit C 50-gen-ish gate)
REM   - extinction_rescue_top_k=2 (R2 stage 3 commit 3)
REM
REM Smoke prerequisite: tmp/layer1/smoke_tw_eval.toml passed
REM (verdict HEALTHY, all 9 archetypes mutate+eval running).
REM
REM Expected indicators vs baseline long_run_60gen_2d.toml:
REM   - HEALTHY borderland_metrics_summary verdict
REM   - protagonist b2m promotions every 10 gen (period=10)
REM   - stone deposit / completion events per gen climbing
REM     (curriculum gate persists "stone_unlocked")
REM
REM Estimated runtime: ~90-150 min (borderland eval adds ~30s/gen
REM on top of the ~75s/gen sandbox baseline).

set BIN=D:\claude-code\c++\build_ninja_cuda\bin\neural-eco-protagonist.exe
set CFG=D:\claude-code\c++\tmp\layer1\long_run_60gen_2d_curriculum.toml
set LOG=D:\claude-code\c++\tmp\layer1\run_curriculum_60gen.log
cd /d D:\claude-code\c++
"%BIN%" --config "%CFG%" > "%LOG%" 2>&1
