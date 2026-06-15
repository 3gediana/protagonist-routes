@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d D:\claude-code\c++\routes\deep_protagonist\build_d122
del /q "CMakeFiles\deep_protagonist_train.dir\src\env\Environment.cpp.obj" 2>nul
ninja deep_protagonist_train
echo NINJA_EXIT=%errorlevel%
