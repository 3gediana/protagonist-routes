# Configure deep_protagonist with CMake + Ninja + MSVC + CUDA
# Usage: from project root, run scripts\configure.ps1

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build"

# Find vcvars64.bat. Prefer an English path because cmd's vcvars chokes on
# non-ASCII directory names (e.g. "E:\c语言\..."). Try known good locations
# first, then fall back to vswhere.
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
if (-not $vsPath) {
    $vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -property installationPath
    }
}
if (-not $vsPath) { throw "Visual Studio 2022 not found" }
$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found at $vcvars" }

# Use the cmake bundled with VS to avoid PATH issues with stray mingw cmake
$vsCmake = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path $vsCmake)) {
    Write-Warning "VS-bundled cmake not found, falling back to whatever 'cmake' resolves to"
    $vsCmake = "cmake"
}

# Find CUDA Toolkit
$cudaCandidates = @(
    "D:\NVIDIA\CUDA\v12.8",
    "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.8",
    "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6",
    "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.4"
)
$cudaRoot = $null
foreach ($c in $cudaCandidates) {
    if (Test-Path (Join-Path $c "bin\nvcc.exe")) { $cudaRoot = $c; break }
}
if (-not $cudaRoot) { throw "CUDA Toolkit not found in known locations" }

# Convert paths to CMake-friendly forward slashes
$cudaRootFs = $cudaRoot.Replace('\','/')
$rootFs = $root.Replace('\','/')
$buildDirFs = $buildDir.Replace('\','/')

if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir | Out-Null }

Write-Host "Using VS at:    $vsPath"
Write-Host "Using cmake:    $vsCmake"
Write-Host "Using CUDA at:  $cudaRoot"
Write-Host "Configuring with Ninja + MSVC..."

# Hand off to a .bat script via environment variables. Mixing vcvars + cmake
# in one cmd /c "..." line is fragile because PowerShell's quoting interacts
# with cmd's quoting, and %VAR% expansion timing depends on the parser.
$env:VS_VCVARS = $vcvars
$env:VS_CMAKE  = $vsCmake
$env:CUDA_ROOT = $cudaRoot
$env:SRC_DIR   = $rootFs
$env:BUILD_DIR = $buildDirFs

$innerBat = Join-Path $PSScriptRoot "_configure_inner.bat"
& cmd /c "$innerBat"
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

Write-Host ""
Write-Host "Configure done. Build with: scripts\build.ps1"
