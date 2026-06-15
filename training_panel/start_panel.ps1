# start_panel.ps1 — one-click launcher for the DP/PE training panel.
#
# Installs the (tiny) Python deps into a local venv on first run, then starts
# the FastAPI server bound to localhost. Open http://127.0.0.1:8070 after it
# prints "Uvicorn running".
#
# Usage:  ./start_panel.ps1            # default port 8070
#         ./start_panel.ps1 -Port 9000

param([int]$Port = 8070)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $here

$venv = Join-Path $here ".venv"
$py = Join-Path $venv "Scripts\python.exe"

if (-not (Test-Path $py)) {
    Write-Host "[panel] creating venv ..." -ForegroundColor Cyan
    python -m venv $venv
    & $py -m pip install --quiet --upgrade pip
    & $py -m pip install --quiet "fastapi>=0.110" "uvicorn[standard]>=0.29" "psutil>=5.9"
}

$env:PANEL_PORT = "$Port"
Write-Host "[panel] starting on http://127.0.0.1:$Port  (Ctrl+C to stop)" -ForegroundColor Green
& $py -m uvicorn server:app --host 127.0.0.1 --port $Port
