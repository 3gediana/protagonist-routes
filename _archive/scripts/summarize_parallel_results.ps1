$pe = 'D:\claude-code\c++\routes\protagonist_ecology'
$dp = 'D:\claude-code\c++\routes\deep_protagonist'
Write-Output '=== GPU / processes ==='
nvidia-smi --query-gpu=memory.used,memory.total,utilization.gpu --format=csv,noheader,nounits
Get-CimInstance Win32_Process | Where-Object { $_.Name -match 'neural-eco|deep_protagonist' } | Select-Object ProcessId,Name,CommandLine | Format-Table -AutoSize

function Parse-PE($file) {
  if(!(Test-Path $file)) { return $null }
  $lines = @(Select-String -Path $file -Pattern 'Gen \d+ \| protagonist.*me_cov=' | ForEach-Object {$_.Line})
  if($lines.Count -eq 0){ return $null }
  $first = $lines[0]; $last = $lines[-1]
  $mf = [regex]::Match($first,'Gen (\d+).*builds=(\d+) chops=(\d+) hunts=(\d+).*me_cov=(\d+)/64 qd=([0-9.]+)')
  $ml = [regex]::Match($last,'Gen (\d+).*builds=(\d+) chops=(\d+) hunts=(\d+).*me_cov=(\d+)/64 qd=([0-9.]+)')
  if(!$mf.Success -or !$ml.Success){ return $null }
  $maxH = 0; $maxC = 0; $maxCov = 0
  foreach($ln in $lines){
    $m=[regex]::Match($ln,'builds=(\d+) chops=(\d+) hunts=(\d+).*me_cov=(\d+)/64 qd=([0-9.]+)')
    if($m.Success){
      $h=[int]$m.Groups[3].Value; $c=[int]$m.Groups[2].Value; $cov=[int]$m.Groups[4].Value
      if($h -gt $maxH){$maxH=$h}; if($c -gt $maxC){$maxC=$c}; if($cov -gt $maxCov){$maxCov=$cov}
    }
  }
  return [pscustomobject]@{
    File=(Split-Path $file -Leaf); Count=$lines.Count;
    FirstGen=[int]$mf.Groups[1].Value; LastGen=[int]$ml.Groups[1].Value;
    Cov0=[int]$mf.Groups[5].Value; Cov1=[int]$ml.Groups[5].Value; MaxCov=$maxCov;
    QD0=[double]$mf.Groups[6].Value; QD1=[double]$ml.Groups[6].Value;
    Builds=[int]$ml.Groups[2].Value; Chops=[int]$ml.Groups[3].Value; Hunts=[int]$ml.Groups[4].Value;
    MaxChops=$maxC; MaxHunts=$maxH
  }
}

Write-Output '=== PE results ==='
$peFiles = @()
$patterns = @('onepot_sD_shortrun_*.log','onepot_sD_longrun_*.log','onepot_sD_explore_*.log','onepot_sD_huntwide_*.log','onepot_sD_hybrid_*.log')
foreach($pat in $patterns){ $peFiles += @(Get-ChildItem $pe -Filter $pat -File -ErrorAction SilentlyContinue) }
$rows = @()
foreach($f in $peFiles){ $r=Parse-PE $f.FullName; if($r -ne $null){ $rows += $r } }
$rows | Sort-Object File | Format-Table -AutoSize

Write-Output '=== DP stdout tail ==='
Get-ChildItem "$dp\runs" -Filter 'ft_hyp_*.stdout.log' -File -ErrorAction SilentlyContinue | ForEach-Object {
  Write-Output "--- $($_.Name) ---"
  Get-Content $_.FullName -Tail 20
}
Write-Output '=== DP metrics tail ==='
Get-ChildItem "$dp\runs" -Filter 'ft_hyp_*.jsonl' -File -ErrorAction SilentlyContinue | ForEach-Object {
  Write-Output "--- $($_.Name) ---"
  Get-Content $_.FullName -Tail 5
}
Write-Output '=== summaries ==='
if(Test-Path "$pe\pe_parallel_hypotheses_summary.txt"){Get-Content "$pe\pe_parallel_hypotheses_summary.txt" -Tail 30}
if(Test-Path "$dp\runs\dp_hypotheses_log.txt"){Get-Content "$dp\runs\dp_hypotheses_log.txt" -Tail 20}
