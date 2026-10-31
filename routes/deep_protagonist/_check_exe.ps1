$exe = 'D:\claude-code\c++\routes\deep_protagonist\build_devin_20260610\bin\deep_protagonist_train.exe'
$obj1 = 'D:\claude-code\c++\routes\deep_protagonist\build_devin_20260610\CMakeFiles\deep_protagonist_train.dir\src\policy\PPO.cpp.obj'
$e = Get-Item $exe
Write-Output ("EXE lastWrite = " + $e.LastWriteTime + "  size=" + $e.Length)
if (Test-Path $obj1) { $o = Get-Item $obj1; Write-Output ("PPO.cpp.obj lastWrite = " + $o.LastWriteTime + "  size=" + $o.Length) } else { Write-Output "PPO.cpp.obj NOT FOUND (path differs)" }
$hit = Select-String -Path $exe -Pattern 'discrete-head decoupling' -Encoding ascii -SimpleMatch -List
if ($hit) { Write-Output "EXE CONTAINS 'discrete-head decoupling' -> D1 code is compiled in" } else { Write-Output "EXE does NOT contain the D1 string -> stale objects / not compiled in" }
$hit2 = Select-String -Path $exe -Pattern 'decouple-disc' -Encoding ascii -SimpleMatch -List
if ($hit2) { Write-Output "EXE CONTAINS '--decouple-disc' arg string" } else { Write-Output "EXE missing '--decouple-disc' arg string" }
