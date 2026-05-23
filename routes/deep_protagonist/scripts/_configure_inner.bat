@echo off
REM Inner configure script: assumed to be called from configure.ps1 with these
REM environment variables already set:
REM   VS_VCVARS  - full path to vcvars64.bat
REM   CUDA_ROOT  - full path to CUDA toolkit (e.g. D:\NVIDIA\CUDA\v12.8)
REM   VS_CMAKE   - full path to cmake.exe (the VS-bundled one)
REM   SRC_DIR    - project root (forward slashes)
REM   BUILD_DIR  - build directory (forward slashes)

call "%VS_VCVARS%"
if errorlevel 1 (
    echo vcvars64.bat failed
    exit /b 1
)

set "PATH=%CUDA_ROOT%\bin;%PATH%"
set "CUDA_PATH=%CUDA_ROOT%"
set "CUDA_HOME=%CUDA_ROOT%"

REM Convert CUDA path to forward-slash for cmake
set "CUDA_ROOT_FS=%CUDA_ROOT:\=/%"

REM Reuse top-level project's already-cloned third-party sources to avoid
REM relying on GitHub being reachable every time we reconfigure.
set "TOPDEP=D:/claude-code/c++/build_ninja_cuda/_deps"

"%VS_CMAKE%" -G Ninja -S "%SRC_DIR%" -B "%BUILD_DIR%" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCUDAToolkit_ROOT="%CUDA_ROOT_FS%" ^
    -DCUDA_TOOLKIT_ROOT_DIR="%CUDA_ROOT_FS%" ^
    -DFETCHCONTENT_SOURCE_DIR_TOMLPLUSPLUS="%TOPDEP%/tomlplusplus-src" ^
    -DFETCHCONTENT_SOURCE_DIR_SPDLOG="%TOPDEP%/spdlog-src" ^
    -DFETCHCONTENT_SOURCE_DIR_RAYLIB="%TOPDEP%/raylib-src"
exit /b %errorlevel%
