$root='D:\claude-code\c++'
$ra=Join-Path $root 'tmp\rhythm_agg.ps1'
$out='D:\claude-code\c++\tmp\trace_export'
if(-not (Test-Path $out)){ New-Item -ItemType Directory -Path $out | Out-Null }
$gens=140,145,150,155,159
foreach($seed in 50001,50002,50003){ foreach($oo in 'on','off'){
  $glob="realism_detP2c_th22_"+$seed+"_"+$oo
  foreach($g in $gens){
    & $ra -RunGlob $glob -Gen ("generation_"+$g) -Label ("c_"+$seed+"_"+$oo+"_g"+$g)
  }
}}
Write-Output '=== detP2c agg files built ==='
Get-ChildItem $out -Filter 'agg_c_*.csv' | Select-Object Name,Length | ForEach-Object { Write-Output ($_.Name+' '+$_.Length) }
