<#
night_launch.ps1 — 睡前一键：把 night_run.ps1 注册成 schtasks 后台任务并启动

这是唯一会真正"开跑"的脚本。设计成两步保险：
  1) 默认（不带 -Go）只做预检 + 打印将要跑的队列和确切命令，不启动任何训练。
  2) 确认无误后加 -Go 才真正 schtasks /create + /run（脱离 ssh，断连不死）。

用法：
  # 先空跑看队列（不启动）：
  powershell -ExecutionPolicy Bypass -File night_launch.ps1
  # 确认后真正启动：
  powershell -ExecutionPolicy Bypass -File night_launch.ps1 -Go

参数：
  -Manifest  实验清单（默认 night_manifest.txt）
  -Steps     每臂步数（默认 10000000）
  -NEnvs     向量化环境数（默认 8）
  -Concurrency 并发池同时跑几臂（默认 2；由 probe_tput 实测吞吐选定）
  -TaskName  schtasks 任务名（默认 dp_night）
  -Go        真正创建并启动任务（不带则只预检/打印）
#>
param(
    [string]$Manifest    = "night_manifest.txt",
    [int]   $Steps       = 10000000,
    [int]   $NEnvs       = 8,
    [int]   $Concurrency = 2,
    [string]$TaskName    = "dp_night",
    [switch]$Go
)

$ErrorActionPreference = "Stop"
$proj   = "D:\claude-code\c++\routes\deep_protagonist"
$exe    = Join-Path $proj "build_devin_20260610\bin\deep_protagonist_train.exe"
$runner = Join-Path $proj "night_run.ps1"
$manifestPath = if ([System.IO.Path]::IsPathRooted($Manifest)) { $Manifest } else { Join-Path $proj $Manifest }
$tag    = Get-Date -Format "yyyyMMdd-HHmmss"

Write-Output "=== night_launch 预检 ==="

# 1) 文件存在性
foreach ($f in @($exe, $runner, $manifestPath)) {
    if (Test-Path $f) { Write-Output "  OK   $f" }
    else { Write-Output "  FAIL 缺文件: $f"; exit 1 }
}

# 2) 没有别的训练在跑（避免抢 8GB 显存）
$running = Get-Process deep_protagonist_train -ErrorAction SilentlyContinue
if ($running) {
    Write-Output "  FAIL 已有 deep_protagonist_train 在跑 (PID $($running.Id -join ','))；先停掉再启动夜跑。"
    exit 1
} else { Write-Output "  OK   当前无训练进程" }

# 3) GPU 余量（8GB 卡；打游戏/挂壁纸会抢显存）
$free = (nvidia-smi --query-gpu=memory.free --format=csv,noheader,nounits | Select-Object -First 1).Trim()
Write-Output "  GPU free: $free MiB（单组 n_envs=8 训练通常占 1-2GB；<2000 建议先关游戏/壁纸引擎）"

# 4) 解析并打印队列
$arms = @()
foreach ($raw in Get-Content $manifestPath) {
    $line = $raw.Trim()
    if ($line -eq "" -or $line.StartsWith("#")) { continue }
    $bar = $line.IndexOf("|"); if ($bar -lt 0) { continue }
    $arms += [pscustomobject]@{ Name = $line.Substring(0,$bar).Trim(); Flags = $line.Substring($bar+1).Trim() }
}
# 并发池：约 ceil(臂数/并发) 批，每批 ~1h（10M）；并发会让单臂稍慢，留 1.3x 余量
$batches = [math]::Ceiling($arms.Count / [double]$Concurrency)
$estH = [math]::Round($batches * ($Steps / 10000000.0) * 1.3, 1)
Write-Output ""
Write-Output "队列共 $($arms.Count) 组，每组 $Steps 步，并发 $Concurrency，预计 ~${estH} 小时跑完（${batches} 批 x ~1h x 1.3 余量）："
$i = 0
foreach ($a in $arms) { $i++; Write-Output ("  {0,2}. {1,-22} {2}" -f $i, $a.Name, $a.Flags) }

$runArgs = "-ExecutionPolicy Bypass -NoProfile -File `"$runner`" -Manifest `"$manifestPath`" -Steps $Steps -NEnvs $NEnvs -Concurrency $Concurrency -Tag $tag"
Write-Output ""
Write-Output "将注册的计划任务（Task Scheduler，后台、断 ssh 不死）："
Write-Output "  Register-ScheduledTask -TaskName $TaskName  (powershell $runArgs)"
Write-Output "  Start-ScheduledTask -TaskName $TaskName"

if (-not $Go) {
    Write-Output ""
    Write-Output ">>> 这是预检模式，未启动任何训练。确认队列无误后，重跑并加 -Go 才会真正开跑。"
    exit 0
}

# ---- 真正启动（Register-ScheduledTask，规避 schtasks /tr 的嵌套引号坑）----
Write-Output ""
Write-Output ">>> -Go 已指定，注册并启动计划任务 $TaskName ..."
$action  = New-ScheduledTaskAction -Execute "powershell.exe" -Argument $runArgs -WorkingDirectory $proj
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit ([TimeSpan]::Zero)
Register-ScheduledTask -TaskName $TaskName -Action $action -Settings $settings -Force | Out-Null
Start-ScheduledTask -TaskName $TaskName
Write-Output "已启动 (tag=$tag)。监控/停止："
Write-Output "  状态:  Get-Content night_status_$tag.txt -Wait    （或  Get-ScheduledTask $TaskName）"
Write-Output "  进度:  Get-Process deep_protagonist_train"
Write-Output "  停止:  Stop-ScheduledTask -TaskName $TaskName ; taskkill /IM deep_protagonist_train.exe /F"
Write-Output "  早上验收:  python3 scripts\night_analyze.py --metrics-dir metrics --manifest night_manifest.txt"
