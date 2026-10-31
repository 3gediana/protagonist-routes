param(
  [string]$tag,
  [string]$envsets = "",   # e.g. "DP_SPEAR_REACH=3.5;DP_RAB_STAM=1.5"
  [string]$extra   = "",   # extra CLI args, e.g. "--ppo-hunt-shaping 1.0"
  [int]$nenvs      = 4,
  [int]$seed       = 24
)
$ErrorActionPreference = 'Stop'
$dp  = 'D:\claude-code\c++\routes\deep_protagonist'
$exe = "$dp\build_d122_v2\bin\deep_protagonist_train.exe"
$out = "$dp\runs\hunt_$tag.stdout.log"
$err = "$dp\runs\hunt_$tag.stderr.log"
Remove-Item $out,$err -ErrorAction SilentlyContinue

# Build the env-var prefix as chained `set` commands inside cmd /c.
$setpfx = ""
if ($envsets -ne "") {
  foreach ($kv in $envsets.Split(";")) {
    if ($kv.Trim() -ne "") { $setpfx += ('set ' + $kv.Trim() + '&& ') }
  }
}

$argline = '--policy ppo --load runs\r1_cook_s24.pt --weather --allow-cook ' +
           '--ent-coef 0.0015 --critic-warmup 32 --lr 1.5e-4 --ppo-target-kl 0.08 ' +
           '--ppo-hunt-shaping 0.5 ' + $extra + ' ' +
           "--n-envs $nenvs --steps 8000000 --save-path runs\hunt_$tag.pt --save-every 25 " +
           "--metrics runs\hunt_$tag.jsonl --seed $seed"

$cmdline = 'cmd /c "' + $setpfx + '"' + $exe + '" ' + $argline + ' > "' + $out + '" 2> "' + $err + '""'
$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{
        CommandLine      = $cmdline
        CurrentDirectory = $dp
     }
Write-Output ("tag=$tag ReturnValue=" + $r.ReturnValue + " PID=" + $r.ProcessId)
