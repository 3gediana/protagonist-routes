@echo off
REM Build the test_core target then run it.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo vcvars64.bat failed
    exit /b 1
)
cmake --build D:\claude-code\c++\build_ninja_cuda --target test_core
if errorlevel 1 (
    echo BUILD_FAILED
    exit /b 1
)
echo ===RUNNING_TESTS===
D:\claude-code\c++\build_ninja_cuda\bin\test_core.exe
exit /b %errorlevel%
