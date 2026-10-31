@echo off
REM Devin PE build helper: set up VS2022 x64 dev env (MSVC + CUDA) then run the
REM project's bundled ninja in the CUDA build dir. Pass ninja args through, e.g.
REM   _devin_build.bat bin/neural-eco-protagonist-batched.exe
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
  echo [devin_build] vcvars64 failed
  exit /b 1
)
set "NINJA=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
cd /d D:\claude-code\c++\build_ninja_cuda || exit /b 1
"%NINJA%" %*
