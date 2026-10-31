$ErrorActionPreference = 'Stop'
$dp = 'D:\claude-code\c++\routes\deep_protagonist'
$pe = 'D:\claude-code\c++\routes\protagonist_ecology'

Write-Output "=== GPU ==="
nvidia-smi --query-gpu=memory.used,memory.total,utilization.gpu --format=csv,noheader,nounits

Write-Output "`n=== DP ft_rep_bcgen.jsonl ==="
$f = "$dp\runs\ft_rep_bcgen.jsonl"
if (Test-Path $f) {
    $sz = (Get-Item $f).Length
    $lines = Get-Content $f
    $n = $lines.Count
    Write-Output "Size=${sz}B Lines=$n"
    if ($n -gt 0) {
        Write-Output "FIRST: $($lines[0])"
        Write-Output "LAST:  $($lines[-1])"
    }
} else {
    Write-Output "NOT FOUND YET"
    # Check stderr
    $errlog = "$dp\runs\ft_rep_bcgen.stderr.log"
    if (Test-Path $errlog) {
        Write-Output "STDERR:"
        Get-Content $errlog | Select-Object -Last 10
    }
    $outlog = "$dp\runs\ft_rep_bcgen.stdout.log"
    if (Test-Path $outlog) {
        Write-Output "STDOUT:"
        Get-Content $outlog | Select-Object -Last 10
    }
}

Write-Output "`n=== PE seed status ==="
foreach ($seed in @(50021, 50022, 50023, 50024, 50025)) {
    $log = "$pe\onepot_sD_warm_$seed.log"
    if (Test-Path $log) {
        $sz = (Get-Item $log).Length
        $lastLine = Get-Content $log | Select-Object -Last 2
        Write-Output "seed=${seed} logSize=${sz}B"
        $lastLine | ForEach-Object { Write-Output "  $_" }
    } else {
        Write-Output "seed=${seed} NO LOG"
    }
}

Write-Output "`n=== Process count ==="
$dp_n = (Get-Process -Name '*deep_protagonist*' -ErrorAction SilentlyContinue | Measure-Object).Count
$pe_n = (Get-Process -Name '*neural-eco*' -ErrorAction SilentlyContinue | Measure-Object).Count
Write-Output "DP=$dp_n PE=$pe_n"
