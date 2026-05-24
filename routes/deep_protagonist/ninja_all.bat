@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d D:\claude-code\c++\routes\deep_protagonist\build
G:\ai\qtdownload\Tools\Ninja\ninja.exe deep_protagonist deep_protagonist_train 1>ninja_all_out.txt 2>ninja_all_err.txt
echo EXIT=%ERRORLEVEL%
