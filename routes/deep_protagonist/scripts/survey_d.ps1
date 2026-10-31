$root = "D:\claude-code\c++\routes\deep_protagonist"
Write-Output "==== deep_protagonist subdir sizes (GB) ===="
Get-ChildItem -Force -Directory $root -ErrorAction SilentlyContinue | ForEach-Object {
    $s = (Get-ChildItem -Recurse -Force -File -ErrorAction SilentlyContinue $_.FullName | Measure-Object Length -Sum).Sum
    [PSCustomObject]@{ GB = [math]::Round($s/1GB,3); Dir = $_.Name }
} | Sort-Object GB -Descending | Select-Object -First 25 | Format-Table -AutoSize | Out-String -Width 200

Write-Output "==== D:\ top-level sizes (GB) ===="
Get-ChildItem -Force -Directory "D:\" -ErrorAction SilentlyContinue | ForEach-Object {
    $s = (Get-ChildItem -Recurse -Force -File -ErrorAction SilentlyContinue $_.FullName | Measure-Object Length -Sum).Sum
    [PSCustomObject]@{ GB = [math]::Round($s/1GB,2); Dir = $_.FullName }
} | Sort-Object GB -Descending | Select-Object -First 15 | Format-Table -AutoSize | Out-String -Width 200

Write-Output "==== runs/ subdir sizes (GB) ===="
$runs = Join-Path $root "runs"
Get-ChildItem -Force -Directory $runs -ErrorAction SilentlyContinue | ForEach-Object {
    $s = (Get-ChildItem -Recurse -Force -File -ErrorAction SilentlyContinue $_.FullName | Measure-Object Length -Sum).Sum
    [PSCustomObject]@{ GB = [math]::Round($s/1GB,3); Dir = $_.Name }
} | Sort-Object GB -Descending | Select-Object -First 20 | Format-Table -AutoSize | Out-String -Width 200

Write-Output "==== runs/ top-level .pt/.opt total (GB) ===="
$pt = (Get-ChildItem -Force -File $runs -ErrorAction SilentlyContinue | Where-Object { $_.Extension -in ".pt",".opt" } | Measure-Object Length -Sum).Sum
Write-Output ("runs top-level .pt/.opt: " + [math]::Round($pt/1GB,3) + " GB")
