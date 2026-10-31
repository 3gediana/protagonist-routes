$dp = 'D:\claude-code\c++\routes\deep_protagonist'
$bdir = Join-Path $dp 'build_devin_20260610\CMakeFiles\deep_protagonist_train.dir\src'
$src = Join-Path $dp 'src'
$pairs = @(
  @{ s = (Join-Path $src 'main_train.cpp');      o = (Join-Path $bdir 'main_train.cpp.obj') },
  @{ s = (Join-Path $src 'policy\PPO.cpp');       o = (Join-Path $bdir 'policy\PPO.cpp.obj') }
)
foreach ($p in $pairs) {
  $smt = if (Test-Path $p.s) { (Get-Item $p.s).LastWriteTime } else { 'NO SRC' }
  $omt = if (Test-Path $p.o) { (Get-Item $p.o).LastWriteTime } else { 'NO OBJ' }
  Write-Output ("SRC " + $smt + "   OBJ " + $omt + "   " + (Split-Path $p.o -Leaf))
  if (Test-Path $p.o) { Remove-Item -LiteralPath $p.o -Force; Write-Output ("  deleted -> " + $p.o) }
}
# also nuke the hpp-dependent timestamp confusion: touch the two sources so mtime > everything
foreach ($p in $pairs) { if (Test-Path $p.s) { (Get-Item $p.s).LastWriteTime = Get-Date } }
Write-Output 'objs deleted + sources touched; ready to rebuild'
