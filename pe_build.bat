@echo off
REM PE build: set up MSVC env via vcvars64 then ninja-build build_ninja_cuda.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    echo vcvars64.bat failed
    exit /b 1
)
cmake --build D:\claude-code\c++\build_ninja_cuda --target neural-eco-protagonist-batched
exit /b %errorlevel%
