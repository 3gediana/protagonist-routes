param([string]$seeds="2300105",[int]$eps=6,[int]$steps=70000,[string]$dir="night_dagger_ff")
Set-Location 'D:\claude-code\c++\routes\deep_protagonist'
$exe='build\bin\deep_protagonist_train.exe'
New-Item -ItemType Directory -Force -Path "runs\$dir" | Out-Null
Remove-Item "runs\$dir\ALLDONE.txt" -ErrorAction SilentlyContinue
foreach ($s in ($seeds -split ',')) {
  $s=$s.Trim(); if ($s -eq "") { continue }
  $out = "runs\$dir\night_$s.bin"
  $m   = "runs\$dir\night_$s.jsonl"
  Remove-Item $out,$m -ErrorAction SilentlyContinue
  & $exe --policy ppo --load runs\bc_v9.pt --dagger --no-update --oracle-no-fire `
         --record-demos $out --metrics $m --seed $s --steps $steps --episodes $eps `
         *> "runs\$dir\night_$s.log"
  "done seed=$s exit=$LASTEXITCODE" | Out-File "runs\$dir\progress.txt" -Append -Encoding ASCII
}
"ALLDONE $(Get-Date -Format o)" | Out-File "runs\$dir\ALLDONE.txt" -Encoding ASCII
