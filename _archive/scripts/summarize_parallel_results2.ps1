$pe = 'D:\claude-code\c++\routes\protagonist_ecology'
$dp = 'D:\claude-code\c++\routes\deep_protagonist'
Write-Output '=== GPU ==='
nvidia-smi --query-gpu=memory.used,memory.total,utilization.gpu --format=csv,noheader,nounits
Write-Output '=== process command names ==='
Get-CimInstance Win32_Process | Where-Object { $_.Name -match 'neural-eco|deep_protagonist' } | ForEach-Object { "$($_.ProcessId) $($_.Name) $($_.CommandLine)" }
function Parse-PE($file) {
  if(!(Test-Path $file)) { return $null }
  $lines = @(Select-String -Path $file -Pattern 'Gen \d+ \| protagonist.*me_cov=' | ForEach-Object {$_.Line})
  if($lines.Count -eq 0){ return $null }
  $first = $lines[0]; $last = $lines[-1]
  $re='Gen (\d+).*builds=(\d+) chops=(\d+) hunts=(\d+).*me_cov=(\d+)/(\d+) qd=([0-9.]+)'
  $mf = [regex]::Match($first,$re); $ml = [regex]::Match($last,$re)
  if(!$mf.Success -or !$ml.Success){ return $null }
  $maxH = 0; $maxC = 0; $maxCov = 0; $sumH=0; $sumC=0
  foreach($ln in $lines){
    $m=[regex]::Match($ln,$re)
    if($m.Success){
      $h=[int]$m.Groups[4].Value; $c=[int]$m.Groups[3].Value; $cov=[int]$m.Groups[5].Value
      $sumH += $h; $sumC += $c
      if($h -gt $maxH){$maxH=$h}; if($c -gt $maxC){$maxC=$c}; if($cov -gt $maxCov){$maxCov=$cov}
    }
  }
  $pct0=[math]::Round(100*[int]$mf.Groups[5].Value/[int]$mf.Groups[6].Value,1)
  $pct1=[math]::Round(100*[int]$ml.Groups[5].Value/[int]$ml.Groups[6].Value,1)
  return [pscustomobject]@{
    File=(Split-Path $file -Leaf); Count=$lines.Count; FirstGen=[int]$mf.Groups[1].Value; LastGen=[int]$ml.Groups[1].Value;
    Cov=('{0}/{1}->{2}/{3}' -f $mf.Groups[5].Value,$mf.Groups[6].Value,$ml.Groups[5].Value,$ml.Groups[6].Value); Pct=('{0}->{1}%' -f $pct0,$pct1);
    MaxCov=$maxCov; QD=('{0}->{1}' -f $mf.Groups[7].Value,$ml.Groups[7].Value); Builds=[int]$ml.Groups[2].Value; Chops=[int]$ml.Groups[3].Value; Hunts=[int]$ml.Groups[4].Value; MaxChops=$maxC; MaxHunts=$maxH; AvgHunts=[math]::Round($sumH/$lines.Count,1)
  }
}
Write-Output '=== PE all results ==='
$patterns=@('onepot_sD_shortrun_*.log','onepot_sD_longrun_*.log','onepot_sD_explore_*.log','onepot_sD_huntwide_*.log','onepot_sD_hybrid_*.log')
$rows=@(); foreach($pat in $patterns){Get-ChildItem $pe -Filter $pat -File -ErrorAction SilentlyContinue | %{$r=Parse-PE $_.FullName; if($r){$rows+=$r}}}
$rows | Sort-Object File | Format-Table -AutoSize
Write-Output '=== DP metrics file sizes and tails ==='
Get-ChildItem "$dp\runs" -Filter 'ft_hyp_*.jsonl' -File -ErrorAction SilentlyContinue | ForEach-Object { Write-Output "--- $($_.Name) len=$($_.Length) ---"; Get-Content $_.FullName -Tail 3 }
Write-Output '=== DP stderr tails ==='
Get-ChildItem "$dp\runs" -Filter 'ft_hyp_*.stderr.log' -File -ErrorAction SilentlyContinue | ForEach-Object { Write-Output "--- $($_.Name) len=$($_.Length) ---"; Get-Content $_.FullName -Tail 20 }
