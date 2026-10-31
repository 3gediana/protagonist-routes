$ErrorActionPreference='SilentlyContinue'
$pe='D:\claude-code\c++\routes\protagonist_ecology'
Set-Location $pe
Write-Output '=== files matching map/elites/coverage/fitness ==='
Get-ChildItem -Recurse -File -Include '*.cpp','*.h','*.hpp','*.cc','*.py','*.toml' | Where-Object { $_.FullName -notmatch '\\data\\runs\\|\\build' } | Select-String -Pattern 'me_coverage|MAP|MapElite|map elite|descriptor|behavior descriptor|niche|qd_score|quality diversity|coverage' | Select-Object -First 200 Path,LineNumber,Line | Format-List
