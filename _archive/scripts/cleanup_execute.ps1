$ErrorActionPreference='Continue'
$root='D:\claude-code\c++'
$peRuns='D:\claude-code\c++\routes\protagonist_ecology\data\runs'
$manifest=Join-Path $root ('CLEANUP_20260608_manifest_{0}.txt' -f (Get-Date -Format 'HHmmss'))
function Log($msg){ $line="$(Get-Date -Format s) $msg"; Add-Content $manifest $line; Write-Output $line }
function SizeGBPath($path){ if(!(Test-Path $path)){ return 0 }; $s=(Get-ChildItem $path -Recurse -File -Force -ErrorAction SilentlyContinue | Measure-Object Length -Sum).Sum; if($null -eq $s){$s=0}; return [math]::Round($s/1GB,3) }
function FreeGB(){ $d=Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='D:'"; return [math]::Round($d.FreeSpace/1GB,2) }
Log "START freeGB=$(FreeGB)"
Log 'Policy: preserve latest checkpoint_gen per timestamp/run, preserve metrics/config/best genome files; delete PE trace dirs and older intermediate checkpoint dirs.'

# 1) Delete PE trace dirs (visual/debug traces; metrics and checkpoints remain)
$traceDirs=@(Get-ChildItem $peRuns -Recurse -Directory -Filter 'trace' -ErrorAction SilentlyContinue)
foreach($td in $traceDirs){
  $gb=SizeGBPath $td.FullName
  Log "DELETE_TRACE gb=$gb path=$($td.FullName)"
  Remove-Item $td.FullName -Recurse -Force -ErrorAction SilentlyContinue
}
Log "AFTER_TRACE freeGB=$(FreeGB)"

# 2) For each timestamp dir, keep only highest generation checkpoint
$timestampDirs=@()
Get-ChildItem $peRuns -Directory -ErrorAction SilentlyContinue | ForEach-Object {
  $seedDir=$_.FullName
  Get-ChildItem $seedDir -Directory -ErrorAction SilentlyContinue | ForEach-Object { $timestampDirs += $_ }
}
foreach($ts in $timestampDirs){
  $ckpts=@(Get-ChildItem $ts.FullName -Directory -Filter 'checkpoint_gen*' -ErrorAction SilentlyContinue | Where-Object { [regex]::IsMatch($_.Name,'checkpoint_gen\d+') } | Sort-Object { [int]([regex]::Match($_.Name,'checkpoint_gen(\d+)').Groups[1].Value) })
  if($ckpts.Count -le 1){ continue }
  $keep=$ckpts[-1]
  Log "KEEP_LATEST path=$($keep.FullName) count=$($ckpts.Count)"
  $ckpts | Select-Object -First ($ckpts.Count-1) | ForEach-Object {
    $gb=SizeGBPath $_.FullName
    Log "DELETE_OLD_CKPT gb=$gb path=$($_.FullName)"
    Remove-Item $_.FullName -Recurse -Force -ErrorAction SilentlyContinue
  }
}
Log "AFTER_OLD_CKPTS freeGB=$(FreeGB)"

# 3) Delete failed DP first attempts with empty metrics (0-byte jsonl, no useful checkpoint)
$dpRuns='D:\claude-code\c++\routes\deep_protagonist\runs'
Get-ChildItem $dpRuns -File -Filter 'ft_hyp_*.jsonl' -ErrorAction SilentlyContinue | Where-Object { $_.Length -eq 0 } | ForEach-Object {
  $base=$_.BaseName
  foreach($suffix in @('.jsonl','.stdout.log','.stderr.log','.pt','.pt.opt')){
    $p=Join-Path $dpRuns ($base+$suffix)
    if(Test-Path $p){ $len=(Get-Item $p).Length; Log "DELETE_FAILED_DP bytes=$len path=$p"; Remove-Item $p -Force -ErrorAction SilentlyContinue }
  }
}
Log "FINAL freeGB=$(FreeGB) manifest=$manifest"
