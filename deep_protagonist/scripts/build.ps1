# Build deep_protagonist using existing build/ directory
# Usage: from project root, run scripts\build.ps1

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build"

if (-not (Test-Path (Join-Path $buildDir "build.ninja"))) {
    throw "build/ is not configured. Run scripts\configure.ps1 first."
}

# Same VS discovery as configure.ps1 - prefer English path
$vsCandidates = @(
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools",
    "C:\Program Files\Microsoft Visual Studio\2022\BuildTools",
    "C:\Program Files\Microsoft Visual Studio\2022\Community",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional",
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise"
)
$vsPath = $null
foreach ($v in $vsCandidates) {
    if (Test-Path (Join-Path $v "VC\Auxiliary\Build\vcvars64.bat")) {
        $vsPath = $v; break
    }
}
if (-not $vsPath) { throw "Visual Studio 2022 not found" }

$env:VS_VCVARS = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
$env:VS_CMAKE  = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$env:BUILD_DIR = $buildDir.Replace('\','/')

$innerBat = Join-Path $PSScriptRoot "_build_inner.bat"
& cmd /c "$innerBat"
if ($LASTEXITCODE -ne 0) { throw "build failed" }

Write-Host ""
Write-Host "Built. Run with: build\bin\deep_protagonist.exe"
