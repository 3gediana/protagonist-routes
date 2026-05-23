@echo off
REM Inner build script. Expects env vars: VS_VCVARS, VS_CMAKE, BUILD_DIR

call "%VS_VCVARS%"
if errorlevel 1 (
    echo vcvars64.bat failed
    exit /b 1
)

"%VS_CMAKE%" --build "%BUILD_DIR%" --config Release -j
exit /b %errorlevel%
