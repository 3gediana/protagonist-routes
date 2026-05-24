@echo off
REM L6 R3 stage 3 commits A-M (including commit L: structural NEAT mutation).
REM 60-gen structural-mutation AB on top of the graduate AB baseline.
REM
REM Enables EVERYTHING the graduate AB enables PLUS:
REM   - borderland_mutate_add_conn_rate = 0.10
REM   - borderland_mutate_add_node_rate = 0.04
REM
REM (sandbox NEAT defaults, mirrored into borderland)
REM
REM Prerequisite: graduate AB (long_run_60gen_2d_curriculum.toml) must
REM have completed first so we have baseline metrics to compare against.
REM
REM Expected indicators vs graduate AB baseline:
REM   - Same harmony_check verdict (7/7 or near-7/7 PASS)
REM   - borderland_metrics_summary.py "Structural mutation: ACTIVE"
REM   - At least one background archetype's node_count_avg /
REM     connection_count_avg grow vs gen 0
REM   - protagonist row remains 0/0 (dense weights have no structure)
REM
REM Estimated runtime: ~90-150 min (structural mutation adds
REM negligible cost over weight perturb; mostly the borderland eval
REM mini-tick is the cost).

set BIN=D:\claude-code\c++\build_ninja_cuda\bin\neural-eco-protagonist.exe
set CFG=D:\claude-code\c++\tmp\layer1\long_run_60gen_2d_structural.toml
set LOG=D:\claude-code\c++\tmp\layer1\run_structural_60gen.log
cd /d D:\claude-code\c++
"%BIN%" --config "%CFG%" > "%LOG%" 2>&1
