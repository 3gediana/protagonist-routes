@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d D:\claude-code\c++\routes\deep_protagonist
cl /nologo /std:c++17 /EHsc /I include test_fire.cpp src\world\FireSystem.cpp /Fe:test_fire.exe >test_fire_build.log 2>&1
echo BUILD_EXIT=%ERRORLEVEL%
test_fire.exe
echo RUN_EXIT=%ERRORLEVEL%
