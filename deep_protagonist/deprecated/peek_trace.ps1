param([string]$tag="cur",[int]$bytes=2000)
$f="D:\claude-code\c++\routes\deep_protagonist\runs\trace_$tag.csv"
if(-not (Test-Path $f)){ Write-Output "NO CSV"; exit }
$fs=[System.IO.File]::Open($f,'Open','Read','ReadWrite')
Write-Output ("bytes=" + $fs.Length)
$n=[Math]::Min($bytes,$fs.Length)
$fs.Seek(-$n,'End') | Out-Null
$b=New-Object byte[] $n
$fs.Read($b,0,$n) | Out-Null
$fs.Close()
[System.Text.Encoding]::ASCII.GetString($b)
