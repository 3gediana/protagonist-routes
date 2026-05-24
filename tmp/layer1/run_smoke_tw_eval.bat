@echo off
REM L6 R3 stage 3 commit E/F smoke verification.
REM Runs the borderland mini-tick evaluator (period=1, every gen)
REM through 4 generations to confirm:
REM   1. spdlog "borderland mini-tick eval updated N genomes" appears
REM   2. No crash from the new makeProtagonistAgent / makeBackgroundAgent
REM      injection paths
REM   3. fitness array in borderland pool updates between gens (visible
REM      in TripleWorld INFO log)
REM Run AFTER the 60-gen throw_penalty AB releases the binary.

set BIN=D:\claude-code\c++\build_ninja_cuda\bin\neural-eco-protagonist.exe
set CFG=D:\claude-code\c++\tmp\layer1\smoke_tw_eval.toml
set LOG=D:\claude-code\c++\tmp\layer1\run_smoke_tw_eval.log
cd /d D:\claude-code\c++
"%BIN%" --config "%CFG%" > "%LOG%" 2>&1
