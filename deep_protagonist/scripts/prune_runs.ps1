# deep_protagonist runs/ auto-prune
# Usage: powershell -ExecutionPolicy Bypass -File scripts\prune_runs.ps1
#
# Rules:
#   - Keep all .pt / .opt / .jsonl / .stdout.log for the latest 5 D-XXX iterations
#   - Keep all gold-standard ckpts (current: D-061, D-065)
#   - Delete everything else matching ppo_d*.*
#   - Untouched: any *.opt.bak* files (manual hide), files NOT matching ppo_d*

param(
    [int]$keepLatest = 5,
    [string[]]$goldKeep = @("d061", "d065", "d076"),  # d076 = NEW GOLD (5M fresh init score 0.312)
    [string]$runsDir = "D:\claude-code\c++\routes\deep_protagonist\runs",
    [switch]$DryRun
)

if (-not (Test-Path $runsDir)) { Write-Error "runs dir missing: $runsDir"; exit 1 }

$ckptRegex = '^ppo_d(\d{3})_'
$allDXXX = Get-ChildItem $runsDir -File -Force | Where-Object { $_.Name -match $ckptRegex } | ForEach-Object {
    if ($_.Name -match $ckptRegex) { [int]$matches[1] }
} | Select-Object -Unique | Sort-Object -Descending

if ($allDXXX.Count -eq 0) { Write-Host "no D-XXX found"; exit 0 }

$latestKeep = $allDXXX | Select-Object -First $keepLatest | ForEach-Object { "d{0:D3}" -f $_ }
$allKeep = ($latestKeep + $goldKeep) | Select-Object -Unique
Write-Host ("keep: " + ($allKeep -join ', '))

$totalDel = 0
Get-ChildItem $runsDir -File -Force | Where-Object { $_.Name -match $ckptRegex } | ForEach-Object {
    if ($_.Name -match $ckptRegex) {
        $dnum = "d{0:D3}" -f [int]$matches[1]
        if ($allKeep -notcontains $dnum -and $_.Name -notmatch "\.bak") {
            $sz = $_.Length
            Write-Host ("  -" + $_.Name + " " + [math]::Round($sz/1MB,2) + "MB")
            if (-not $DryRun) { Remove-Item -LiteralPath $_.FullName -Force }
            $totalDel += $sz
        }
    }
}

$tag = ""
if ($DryRun) { $tag = " [DRY RUN]" }
Write-Host ("reclaimed: " + [math]::Round($totalDel/1MB,1) + " MB" + $tag)
