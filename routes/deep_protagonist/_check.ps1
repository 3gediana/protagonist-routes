$proj = "D:\claude-code\c++\routes\deep_protagonist"
foreach ($f in @("night_run.ps1","night_launch.ps1","night_cpu.ps1")) {
    $p = Join-Path $proj $f
    $errs = $null
    [void][System.Management.Automation.Language.Parser]::ParseFile($p, [ref]$null, [ref]$errs)
    if ($errs -and $errs.Count -gt 0) {
        Write-Output ("{0}: {1} PARSE ERRORS" -f $f, $errs.Count)
        $errs | ForEach-Object { Write-Output ("   " + $_.Message) }
    } else {
        Write-Output ("{0}: OK" -f $f)
    }
}
