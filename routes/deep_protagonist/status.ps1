Write-Output "=== GPU ==="
& 'nvidia-smi' --query-gpu=memory.used,memory.total,utilization.gpu --format=csv,noheader 2>$null
Write-Output "=== DP procs ==="
@(Get-Process deep_protagonist_train -ErrorAction SilentlyContinue).Count
Write-Output "=== PE procs ==="
@(Get-Process neural-eco-protagonist-batched -ErrorAction SilentlyContinue).Count
Write-Output "=== DP cmdlines ==="
Get-CimInstance Win32_Process -Filter "name='deep_protagonist_train.exe'" | ForEach-Object { ($_.CommandLine -split 'hunt_')[1] -split '\.' | Select-Object -First 1 }
