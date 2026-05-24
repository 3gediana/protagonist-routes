@echo off
REM Stone bootstrap commit 1/2/3 + L6 R3 commits A-G end-to-end 60-gen
REM AB. Compared to throw_penalty (which only had throw_attempt=-0.05),
REM this also turns on archetype_progression_persistence_enabled so
REM the protagonist's NurseryProgressionLayer unlock flags survive
REM across episode resets.
REM
REM Run AFTER:
REM   1. The throw_penalty 60-gen AB releases the binary
REM   2. (optional) smoke_tw_eval.bat passes — confirms the new
REM      BorderlandEvaluator wiring doesn't crash in a 4-gen run
REM
REM Expected delta vs throw_penalty baseline: stone deposit /
REM completion events per gen should climb because unlock state
REM persists across episodes once the first protagonist crosses the
REM 6-deposit threshold in episode 0.

set BIN=D:\claude-code\c++\build_ninja_cuda\bin\neural-eco-protagonist.exe
set CFG=D:\claude-code\c++\tmp\layer1\long_run_60gen_2d_curriculum.toml
set LOG=D:\claude-code\c++\tmp\layer1\run_curriculum_60gen.log
cd /d D:\claude-code\c++
"%BIN%" --config "%CFG%" > "%LOG%" 2>&1
