Get-Process -Name 'neural-eco-protagonist-batched','deep_protagonist','deep_protagonist_train' -ErrorAction SilentlyContinue |
  Select-Object Name, Id, @{N='MemMB';E={[math]::Round($_.WorkingSet64/1MB,0)}} |
  Format-Table -AutoSize | Out-String
$os = Get-CimInstance Win32_OperatingSystem
Write-Output ('FreeRAM_GB=' + [math]::Round($os.FreePhysicalMemory/1MB,1) + ' / TotalRAM_GB=' + [math]::Round($os.TotalVisibleMemorySize/1MB,1))
$d = Get-PSDrive D
Write-Output ('D_free_GB=' + [math]::Round($d.Free/1GB,1))
