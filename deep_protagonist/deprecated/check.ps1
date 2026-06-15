$p = "D:\claude-code\c++\routes\deep_protagonist\"
$cpp = $p + "src\env\Environment.cpp"
$obj = $p + "build_d122\CMakeFiles\deep_protagonist_train.dir\src\env\Environment.cpp.obj"
$exe = $p + "build_d122\bin\deep_protagonist_train.exe"
Write-Output ("diag_present=" + ((Select-String -Path $cpp -Pattern 'cook OK' -Quiet) -as [bool]))
Write-Output ("cpp_mtime=" + (Get-Item $cpp).LastWriteTime.ToString('s'))
if (Test-Path $obj) { Write-Output ("obj_mtime=" + (Get-Item $obj).LastWriteTime.ToString('s')) } else { Write-Output "obj_MISSING" }
if (Test-Path $exe) { Write-Output ("exe_mtime=" + (Get-Item $exe).LastWriteTime.ToString('s') + " size=" + (Get-Item $exe).Length) } else { Write-Output "exe_MISSING" }
