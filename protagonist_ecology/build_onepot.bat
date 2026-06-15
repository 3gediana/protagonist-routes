@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d D:\claude-code\c++\build_ninja_cuda
"C:\PROGRA~2\MICROS~2\2022\BUILDT~1\Common7\IDE\COMMON~1\MICROS~1\CMake\Ninja\ninja.exe" neural-eco-protagonist-batched
echo NINJA_EXIT_CODE=%ERRORLEVEL%
