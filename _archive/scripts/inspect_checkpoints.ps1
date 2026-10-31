$peRuns='D:\claude-code\c++\routes\protagonist_ecology\data\runs'
Write-Output '=== PE checkpoint groups ==='
Get-ChildItem $peRuns -Directory -ErrorAction SilentlyContinue | ForEach-Object {
  $seedDir=$_.FullName
  Get-ChildItem $seedDir -Directory -ErrorAction SilentlyContinue | ForEach-Object {
    $ts=$_.FullName
    $ckpts=@(Get-ChildItem $ts -Directory -Filter 'checkpoint_gen*' -ErrorAction SilentlyContinue | Sort-Object Name)
    if($ckpts.Count -gt 0){
      $s=($ckpts | ForEach-Object { (Get-ChildItem $_.FullName -Recurse -File -Force -ErrorAction SilentlyContinue | Measure-Object Length -Sum).Sum } | Measure-Object -Sum).Sum
      $latest=$ckpts | Sort-Object { [int]([regex]::Match($_.Name,'checkpoint_gen(\d+)').Groups[1].Value) } | Select-Object -Last 1
      [pscustomobject]@{SeedDir=(Split-Path $seedDir -Leaf); Timestamp=(Split-Path $ts -Leaf); Count=$ckpts.Count; GB=[math]::Round($s/1GB,2); Latest=$latest.Name; LatestTime=$latest.LastWriteTime}
    }
  }
} | Sort-Object GB -Descending | Format-Table -AutoSize
Write-Output '=== Non-checkpoint files in top run dirs ==='
Get-ChildItem $peRuns -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 5 | ForEach-Object {
  Write-Output "--- $($_.FullName) ---"
  Get-ChildItem $_.FullName -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.FullName -notmatch 'checkpoint_gen' } | Sort-Object Length -Descending | Select-Object -First 10 FullName,@{n='MB';e={[math]::Round($_.Length/1MB,1)}} | Format-Table -AutoSize
}
