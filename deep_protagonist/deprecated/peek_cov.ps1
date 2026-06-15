$dp = 'D:\claude-code\c++\routes\deep_protagonist\runs'
foreach ($tag in @('ja_ctrl','jb_e01','jc_e03','jd_e06','je_e10')) {
  $f = Join-Path $dp ("hunt_" + $tag + ".stdout.log")
  if (-not (Test-Path $f)) { Write-Output ("[" + $tag + "] no log"); continue }
  $fs = [System.IO.File]::Open($f,'Open','Read','ReadWrite')
  $sr = New-Object System.IO.StreamReader($fs)
  $txt = $sr.ReadToEnd(); $sr.Close(); $fs.Close()
  $eps = [regex]::Matches($txt, 'ep (\d+): steps=(\d+).*?explore_cells=(\d+) closest_prey=([\-0-9.]+)m')
  $n = $eps.Count
  if ($n -eq 0) { Write-Output ("[" + $tag + "] 0 episodes yet"); continue }
  $rows = @()
  foreach ($m in $eps) {
    $st = [int]$m.Groups[2].Value
    $cl = [int]$m.Groups[3].Value
    $rows += [pscustomobject]@{ ep=[int]$m.Groups[1].Value; steps=$st; cells=$cl; cp=[double]$m.Groups[4].Value; cpk=([double]$cl/[math]::Max($st,1)*1000) }
  }
  function S($subset) {
    $st=($subset|Measure-Object steps -Average).Average
    $cl=($subset|Measure-Object cells -Average).Average
    $cpk=($subset|Measure-Object cpk -Average).Average
    $cpv=($subset|Where-Object{$_.cp -ge 0}|Measure-Object cp -Minimum).Minimum
    return ("steps=" + [math]::Round($st) + " cells=" + [math]::Round($cl,1) + " cells/1k=" + [math]::Round($cpk,2) + " cpMin=" + [math]::Round($cpv,1) + "m")
  }
  $first = $rows | Select-Object -First 8
  $last  = $rows | Select-Object -Last 8
  Write-Output ("[" + $tag + "] eps=" + $n)
  Write-Output ("    first8: " + (S $first))
  Write-Output ("    last8 : " + (S $last))
}
