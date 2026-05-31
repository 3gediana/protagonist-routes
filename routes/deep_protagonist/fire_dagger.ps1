param([int]$lo=2200110,[int]$hi=2200139,[int]$eps=6,[int]$steps=70000)
Set-Location 'D:\claude-code\c++\routes\deep_protagonist'
$exe='build\bin\deep_protagonist_train.exe'
New-Item -ItemType Directory -Force -Path runs\fire_dagger | Out-Null
for ($s=$lo; $s -le $hi; $s++) {
  $out = "runs\fire_dagger\fire_$s.bin"
  Remove-Item $out -ErrorAction SilentlyContinue
  & $exe --policy ppo --load runs\bc_v9.pt --dagger --no-update `
         --record-demos $out --seed $s --steps $steps --episodes $eps `
         *> "runs\fire_dagger\fire_$s.log"
}
"FIRE_DAGGER_DONE lo=$lo hi=$hi" | Out-File "runs\fire_dagger\done.txt" -Encoding ASCII
