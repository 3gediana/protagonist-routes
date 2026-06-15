param(
  [string]$tag = "cur",
  [string]$envsets = "",   # optional extra env knobs; default = shipped game
  [string]$extra   = ""    # optional extra CLI; default = baseline
)
$ErrorActionPreference = 'Stop'
$dp  = 'D:\claude-code\c++\routes\deep_protagonist'
$exe = "$dp\build_d122_v2\bin\deep_protagonist_train.exe"
$out = "$dp\runs\trace_$tag.stdout.log"
$err = "$dp\runs\trace_$tag.stderr.log"
$csv = "$dp\runs\trace_$tag.csv"
Remove-Item $out,$err,$csv -ErrorAction SilentlyContinue

# DP_TRACE makes ONE env instance dump a per-step behavior row to $csv.
$setpfx = ('set DP_TRACE=' + $csv + '&& ')
if ($envsets -ne "") {
  foreach ($kv in $envsets.Split(";")) {
    if ($kv.Trim() -ne "") { $setpfx += ('set ' + $kv.Trim() + '&& ') }
  }
}

# Baseline = the shipped "current params": resume champion, n-envs 1 so the
# trace is a single clean life, modest steps (a few full episodes), low ent.
$argline = '--policy ppo --load runs\r1_cook_s24.pt --weather --allow-cook ' +
           '--ent-coef 0.0015 --critic-warmup 32 --lr 1.5e-4 --ppo-target-kl 0.08 ' +
           '--ppo-hunt-shaping 0.5 ' + $extra + ' ' +
           "--n-envs 1 --steps 200000 --save-path runs\trace_$tag.pt --save-every 999999 " +
           "--metrics runs\trace_$tag.jsonl --seed 24"

$cmdline = 'cmd /c "' + $setpfx + '"' + $exe + '" ' + $argline + ' > "' + $out + '" 2> "' + $err + '""'
$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{
        CommandLine      = $cmdline
        CurrentDirectory = $dp
     }
Write-Output ("trace tag=$tag ReturnValue=" + $r.ReturnValue + " PID=" + $r.ProcessId + " csv=" + $csv)
