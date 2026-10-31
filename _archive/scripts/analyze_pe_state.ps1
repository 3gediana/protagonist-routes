$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$pe = "D:\claude-code\c++\routes\protagonist_ecology"
Set-Location $pe

function Show-Config($seed) {
  $path = "onepot_sD_warm_$seed.toml"
  Write-Output "=== CONFIG $seed ==="
  Select-String -Path $path -Pattern "random_seed|generations|resume_checkpoint_path|runs_dir|scenario_name|tree|worksite_unlock_deposit_threshold|stone_unlock_deposit_threshold" |
    ForEach-Object { $_.Line }
}

function LatestRun($seed) {
  $root = Join-Path $pe "data\runs\onepot_sD_warm_$seed"
  if (!(Test-Path $root)) { return $null }
  $dirs = Get-ChildItem $root -Directory | Sort-Object LastWriteTime -Descending
  if ($dirs.Count -eq 0) { return $null }
  return $dirs[0].FullName
}

function Show-Run($seed) {
  $run = LatestRun $seed
  Write-Output "=== RUN $seed ==="
  Write-Output $run
  if ($null -eq $run) { return }
  foreach ($csv in @("fitness.csv","protagonist_behaviors.csv","evolution.csv")) {
    $p = Join-Path $run $csv
    if (Test-Path $p) {
      Write-Output "--- $csv first/last ---"
      $rows = Import-Csv $p
      if ($rows.Count -gt 0) {
        $rows[0] | ConvertTo-Json -Compress
        $rows[-1] | ConvertTo-Json -Compress
      }
    } else {
      Write-Output "missing $csv"
    }
  }
  Get-ChildItem $run -Directory -Filter "checkpoint_gen*" | Sort-Object LastWriteTime -Descending |
    Select-Object -First 5 Name,LastWriteTime | Format-Table -AutoSize
}

Show-Config 50021
Show-Config 50025
Show-Run 50025
Show-Run 50021
Show-Run 50022
Show-Run 50023
Show-Run 50024
