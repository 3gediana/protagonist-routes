$root = "D:\claude-code\c++"
Write-Host "=== root-level build/env scripts ==="
Get-ChildItem -Path $root -File | Where-Object { $_.Name -match 'build|vcvars|env|\.bat$|\.ps1$' } | Select-Object -ExpandProperty Name
Write-Host "=== vcvars64.bat candidates ==="
$vc = @(
  "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
  "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)
foreach ($p in $vc) { if (Test-Path $p) { Write-Host "FOUND: $p" } }
Write-Host "=== nvcc on path? ==="
$nv = Get-Command nvcc -ErrorAction SilentlyContinue
if ($nv) { Write-Host ("nvcc: " + $nv.Source) } else { Write-Host "nvcc NOT on PATH" }
Write-Host "=== existing build scripts in repo (recurse, shallow) ==="
Get-ChildItem -Path $root -Recurse -Depth 2 -File -Include *.bat,*.ps1 -ErrorAction SilentlyContinue | Where-Object { $_.Name -match 'build|vcvars|configure' } | Select-Object -ExpandProperty FullName | Select-Object -First 25
