$pe='D:\claude-code\c++\routes\protagonist_ecology'
$rows=@()
foreach($f in Get-ChildItem $pe -File -Filter 'onepot_sD_*.log' | Where-Object {$_.Name -match 'shortrun|explore|huntwide|hybrid'}){
  $txt=Get-Content $f.FullName -Raw
  $run=[regex]::Match($txt,'run_dir=([^,\r\n]+)').Groups[1].Value
  $resume=[regex]::Match($txt,'resumed full checkpoint from ([^\r\n]+) \(generation').Groups[1].Value
  $lines=@(Select-String -Path $f.FullName -Pattern 'Gen \d+ \| protagonist.*me_cov=' | %{$_.Line})
  if($lines.Count -eq 0){continue}
  $last=$lines[-1]
  $m=[regex]::Match($last,'Gen (\d+).*builds=(\d+) chops=(\d+) hunts=(\d+).*me_cov=(\d+)/(\d+) qd=([0-9.]+)')
  $latest=''
  if($run){
    $runAbs=Join-Path $pe $run
    $ck=Get-ChildItem $runAbs -Directory -Filter 'checkpoint_gen*' -ErrorAction SilentlyContinue | Sort-Object { [int]([regex]::Match($_.Name,'checkpoint_gen(\d+)').Groups[1].Value) } | Select-Object -Last 1
    if($ck){$latest=$ck.FullName}
  }
  $rows += [pscustomobject]@{Config=$f.Name; LastGen=$m.Groups[1].Value; Cov=('{0}/{1}' -f $m.Groups[5].Value,$m.Groups[6].Value); QD=$m.Groups[7].Value; Builds=$m.Groups[2].Value; Chops=$m.Groups[3].Value; Hunts=$m.Groups[4].Value; RunDir=$run; KeptCheckpoint=$latest; Resume=$resume}
}
$rows | Sort-Object Config | Format-List
