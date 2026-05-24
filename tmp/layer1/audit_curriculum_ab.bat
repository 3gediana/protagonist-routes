@echo off
REM Post-AB audit batch for the 60-gen graduate run.
REM Run after run_curriculum_60gen.bat completes. Pipes the harmony_check
REM (7 PASS/FAIL gates) and borderland_metrics_summary (HEALTHY / GATED /
REM STAGNANT / INACTIVE verdict + structural-mutation diagnostic) for the
REM most recent run_dir under data\runs\long_run_60gen_2d_curriculum.
REM
REM Resolves the most recent run_dir via PowerShell rather than hard-coding
REM the timestamp. Output is also tee'd into tmp/layer1/audit_curriculum_ab.log
REM for offline reading.

cd /d D:\claude-code\c++

for /f "delims=" %%i in ('powershell -NoProfile -Command "Get-ChildItem 'data\runs\long_run_60gen_2d_curriculum' -Directory | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty FullName"') do set RUN_DIR=%%i

echo === AB run dir: %RUN_DIR% > tmp\layer1\audit_curriculum_ab.log
echo === AB run dir: %RUN_DIR%

echo. >> tmp\layer1\audit_curriculum_ab.log
echo === harmony_check.py === >> tmp\layer1\audit_curriculum_ab.log
echo === harmony_check.py ===
python scripts\protagonist_harmony_check.py "%RUN_DIR%" >> tmp\layer1\audit_curriculum_ab.log 2>&1
python scripts\protagonist_harmony_check.py "%RUN_DIR%"

echo. >> tmp\layer1\audit_curriculum_ab.log
echo === borderland_metrics_summary.py === >> tmp\layer1\audit_curriculum_ab.log
echo.
echo === borderland_metrics_summary.py ===
python scripts\borderland_metrics_summary.py "%RUN_DIR%" >> tmp\layer1\audit_curriculum_ab.log 2>&1
python scripts\borderland_metrics_summary.py "%RUN_DIR%"
