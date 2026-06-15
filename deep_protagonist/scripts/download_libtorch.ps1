# Download libtorch (CUDA 12.8, Release, Windows) and extract to third_party/libtorch
# Usage: powershell -ExecutionPolicy Bypass -File scripts\download_libtorch.ps1

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$thirdParty = Join-Path $root "third_party"
$zipPath = Join-Path $thirdParty "libtorch.zip"
$targetDir = Join-Path $thirdParty "libtorch"
$url = "https://download.pytorch.org/libtorch/cu128/libtorch-win-shared-with-deps-2.11.0%2Bcu128.zip"

if (Test-Path (Join-Path $targetDir "share\cmake\Torch\TorchConfig.cmake")) {
    Write-Host "libtorch already present at $targetDir"
    exit 0
}

if (-not (Test-Path $thirdParty)) { New-Item -ItemType Directory -Path $thirdParty | Out-Null }

if (-not (Test-Path $zipPath)) {
    Write-Host "Downloading libtorch ~2.65GB ..."
    curl.exe -L -o $zipPath $url
    if ($LASTEXITCODE -ne 0) { throw "curl failed with exit code $LASTEXITCODE" }
}

Write-Host "Extracting to $targetDir ..."
Expand-Archive -Path $zipPath -DestinationPath $thirdParty -Force

# the zip extracts as third_party/libtorch/... already, so target_dir is ready
if (-not (Test-Path (Join-Path $targetDir "share\cmake\Torch\TorchConfig.cmake"))) {
    throw "Extraction did not produce expected TorchConfig.cmake at $targetDir"
}

Write-Host "Done. libtorch at $targetDir"
