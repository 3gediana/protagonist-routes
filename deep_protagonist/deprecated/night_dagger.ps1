param([int]$lo=2300100,[int]$hi=2300129,[int]$eps=6,[int]$steps=70000)
Set-Location 'D:\claude-code\c++\routes\deep_protagonist'
$exe='build\bin\deep_protagonist_train.exe'
New-Item -ItemType Directory -Force -Path runs\night_dagger | Out-Null
for ($s=$lo; $s -le $hi; $s++) {
  $out = "runs\night_dagger\night_$s.bin"
  $m   = "runs\night_dagger\night_$s.jsonl"
  Remove-Item $out -ErrorAction SilentlyContinue
  Remove-Item $m   -ErrorAction SilentlyContinue
  & $exe --policy ppo --load runs\bc_v9.pt --dagger --no-update `
         --record-demos $out --metrics $m --seed $s --steps $steps --episodes $eps `
         *> "runs\night_dagger\night_$s.log"
}
"NIGHT_DAGGER_DONE lo=$lo hi=$hi" | Out-File "runs\night_dagger\done.txt" -Encoding ASCII
