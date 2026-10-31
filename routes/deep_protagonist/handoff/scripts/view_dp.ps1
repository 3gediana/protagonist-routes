# view_dp.ps1 - one-click Deep Protagonist web3d replay viewer.
# Starts a local http server over the web3d folder and opens the browser.
# Usage:  ./view_dp.ps1            # default port 8082
#         ./view_dp.ps1 -Port 9002
param([int]$Port = 8082)
$ErrorActionPreference = "Stop"
$viewer = "D:\claude-code\c++\routes\deep_protagonist\viewer\web3d"
if (-not (Test-Path "$viewer\index.html")) { throw "viewer not found: $viewer" }
Set-Location $viewer
$py = (Get-Command python -ErrorAction SilentlyContinue).Source
if (-not $py) { $py = "G:\ai\python11\python.exe" }
$url = "http://127.0.0.1:$Port/index.html"
Write-Host "[view_dp] serving $viewer" -ForegroundColor Cyan
Write-Host "[view_dp] open $url  (Ctrl+C to stop)" -ForegroundColor Green
try { Start-Process $url } catch { Write-Host "[view_dp] could not auto-open browser: $_" -ForegroundColor Yellow }
& $py -m http.server $Port --bind 127.0.0.1
