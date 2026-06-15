$log = 'D:\claude-code\c++\routes\protagonist_ecology\sD_50001.log'
Write-Output '--- lines mentioning a generation number (last 14) ---'
Select-String -Path $log -Pattern 'TripleWorld gen ','Curriculum gen ','injected .* children','MainWorldHost','bridge' |
  Select-Object -Last 14 -ExpandProperty Line
Write-Output '--- any non-Sandbox info/warn lines (last 14) ---'
Get-Content $log | Where-Object { $_ -notmatch 'SandboxDiag|SandboxStatus' } | Select-Object -Last 14
