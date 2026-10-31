$pe = 'D:\claude-code\c++\routes\protagonist_ecology'
& "$pe\clone_pe.ps1" -seed 50011
& "$pe\clone_pe.ps1" -seed 50012
& "$pe\clone_pe.ps1" -seed 50013
Start-Sleep -Seconds 2
Write-Output ("PE running: " + @(Get-Process neural-eco-protagonist-batched -ErrorAction SilentlyContinue).Count)
