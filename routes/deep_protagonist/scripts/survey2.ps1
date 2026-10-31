$cc = "D:\claude-code"
Write-Output "==== build*/node_modules/target dirs under claude-code (GB, top 30) ===="
Get-ChildItem -Recurse -Force -Directory $cc -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match '^(build.*|node_modules|target|\.next|dist|__pycache__)$' } |
    ForEach-Object {
        $s = (Get-ChildItem -Recurse -Force -File -ErrorAction SilentlyContinue $_.FullName | Measure-Object Length -Sum).Sum
        [PSCustomObject]@{ GB = [math]::Round($s/1GB,3); Dir = $_.FullName }
    } | Where-Object { $_.GB -ge 0.2 } | Sort-Object GB -Descending | Select-Object -First 30 |
    Format-Table -AutoSize | Out-String -Width 240

Write-Output "==== active deep_protagonist exe locations + mtime ===="
$dp = "D:\claude-code\c++\routes\deep_protagonist"
Get-ChildItem -Recurse -Force -File $dp -Include "deep_protagonist_train.exe","deep_protagonist.exe" -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending |
    ForEach-Object { Write-Output ($_.LastWriteTime.ToString("yyyy-MM-dd HH:mm") + "  " + [math]::Round($_.Length/1MB,1) + "MB  " + $_.FullName) }
