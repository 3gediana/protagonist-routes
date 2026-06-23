<#
night_cpu.ps1 — 夜间 CPU 旁路（和 GPU 队列并行）

唯一能和 GPU 训练真正并行的 CPU 活：用 scripted/settler oracle 录 demo
（纯 rollout、不训练神经网 → 不抢 8GB 显存），给 BC 暖启动线（C9）攒数据。
注意：训练（PPO/BC）都走 CUDA（use_cuda 写死），CPU 跑不了训练，只能录 demo / 跑分析。

⚠ 首次用先确认它真没 init CUDA / 没抢显存（开着看 nvidia-smi）；若抢了，就别和 GPU 队列并行。

用法（一般也用 schtasks/Register-ScheduledTask 后台跑，和 night_run 并行）：
  powershell -ExecutionPolicy Bypass -File night_cpu.ps1 -Policy scripted -Seeds 7,42,101,202 -Episodes 200

参数：
  -Policy    录制用的脚本策略（scripted | settler，默认 scripted）
  -Seeds     逗号分隔的世界种子，每个种子录一个 demo 文件
  -Episodes  每个种子录多少局（--episodes 上限）
  -OutDir    demo 输出目录（默认 demos）
  -OracleNoFire  录制时禁用 oracle 的 fire 动作（传 --oracle-no-fire）
#>
param(
    [string]$Policy   = "scripted",
    [string]$Seeds    = "7,42,101,202",
    [int]   $Episodes = 200,
    [string]$OutDir   = "demos",
    [switch]$OracleNoFire
)

$ErrorActionPreference = "Stop"
$proj = "D:\claude-code\c++\routes\deep_protagonist"
$exe  = Join-Path $proj "build_devin_20260610\bin\deep_protagonist_train.exe"
Set-Location $proj

$outPath = Join-Path $proj $OutDir
if (-not (Test-Path $outPath)) { New-Item -ItemType Directory -Path $outPath | Out-Null }
if (-not (Test-Path $exe))     { throw "exe 不存在: $exe" }

$tag        = Get-Date -Format "yyyyMMdd-HHmmss"
$statusFile = Join-Path $proj ("night_cpu_status_{0}.txt" -f $tag)
function Log($m) {
    $l = "{0}  {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $m
    Write-Output $l; Add-Content -Path $statusFile -Value $l
}

$seedList = $Seeds -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" }
Log "=== NIGHT_CPU start  policy=$Policy  seeds=$($seedList -join ',')  episodes=$Episodes ==="

foreach ($s in $seedList) {
    $demo = Join-Path $outPath ("oracle_{0}_s{1}.bin" -f $Policy, $s)
    $log  = Join-Path $proj   ("logs\cpu_demo_{0}_s{1}.log" -f $Policy, $s)
    $argList = @("--policy",$Policy,"--seed",$s,"--episodes",$Episodes,"--record-demos",$demo)
    if ($OracleNoFire) { $argList += "--oracle-no-fire" }
    Log ">>> record seed=$s -> $demo"
    $t0 = Get-Date
    & $exe @argList *> $log
    $code = $LASTEXITCODE
    $mins = [math]::Round(((Get-Date) - $t0).TotalMinutes, 1)
    Log "<<< seed=$s EXIT=$code ($mins min)  $demo"
}
Log "=== NIGHT_CPU done  tag=$tag ==="
Log "下一步(GPU,一次性): --policy bc --bc-demos $OutDir\*.bin --bc-out checkpoints\bc.pt --bc-epochs 8 --bc-trigger-weight 5"
