<#
night_run.ps1 — 夜间 GPU 实验编排器（读 manifest，并发池跑队列）

被 night_launch.ps1 注册成 schtasks 任务后台运行（脱离 ssh，断连不死）。
本脚本在 schtasks 进程里前台同步运行；用 Start-Process 起每个臂的子进程，
然后循环监控（HasExited），维持同时 -Concurrency 个臂在跑、完一个补一个，
直到队列跑完才返回。schtasks 父进程一直存活到全部结束，子进程不会成孤儿。

公共 flags 在这里统一补齐，manifest 每行只写"区别于对照"的变量。
统一命名 + 统一 metrics schema => 早上一个 night_analyze.py 能自动遍历对比。
每臂独立进程 + 固定种子；并发只影响速度不影响数值结果，A/B 依旧干净。

用法（一般由 night_launch.ps1 调用；也可手动前台测试）：
  powershell -ExecutionPolicy Bypass -File night_run.ps1 `
      -Manifest night_manifest.txt -Steps 10000000 -NEnvs 8 -Concurrency 3

参数：
  -Manifest     实验清单（默认 night_manifest.txt）
  -Steps        每臂步数（默认 10000000 = 10M）
  -NEnvs        向量化环境数（默认 8）
  -SaveEvery    每多少次 PPO 更新存一次 ckpt（默认 50）
  -Concurrency  同时并发的臂数（默认 2；由 probe 实测吞吐选定）
  -VramFloorMiB 起新臂前要求的最低空闲显存（默认 1200，防 OOM）
  -Tag          本轮批次标签，进 night_status_<Tag>.txt（默认 = 日期时间）
#>
param(
    [string]$Manifest    = "night_manifest.txt",
    [int]   $Steps       = 10000000,
    [int]   $NEnvs       = 8,
    [int]   $SaveEvery   = 50,
    [int]   $Concurrency = 2,
    [int]   $VramFloorMiB = 1200,
    [string]$Tag         = ""
)

$ErrorActionPreference = "Stop"
$proj = "D:\claude-code\c++\routes\deep_protagonist"
$exe  = Join-Path $proj "build_devin_20260610\bin\deep_protagonist_train.exe"

# CUDA 运行环境（与 sfm_ab.bat 一致）
$env:CUDA_PATH = "D:\NVIDIA\CUDA\v12.8"
$env:PATH      = "D:\NVIDIA\CUDA\v12.8\bin;$env:PATH"
Set-Location $proj

if ($Tag -eq "") { $Tag = Get-Date -Format "yyyyMMdd-HHmmss" }
$manifestPath = if ([System.IO.Path]::IsPathRooted($Manifest)) { $Manifest } else { Join-Path $proj $Manifest }
$statusFile   = Join-Path $proj ("night_status_{0}.txt" -f $Tag)

foreach ($d in @("metrics","logs","checkpoints")) {
    $p = Join-Path $proj $d
    if (-not (Test-Path $p)) { New-Item -ItemType Directory -Path $p | Out-Null }
}

if (-not (Test-Path $exe))          { throw "训练 exe 不存在: $exe（先 build_devin_20260610.bat）" }
if (-not (Test-Path $manifestPath)) { throw "manifest 不存在: $manifestPath" }

function Log($msg) {
    $line = "{0}  {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $msg
    Write-Output $line
    Add-Content -Path $statusFile -Value $line
}
function FreeVram() {
    try { return [int]((nvidia-smi --query-gpu=memory.free --format=csv,noheader,nounits | Select-Object -First 1).Trim()) }
    catch { return 99999 }
}
function GpuLine() {
    try { return (nvidia-smi --query-gpu=memory.used,memory.free,utilization.gpu --format=csv,noheader | Select-Object -First 1) }
    catch { return "n/a" }
}

# 解析 manifest -> 待跑队列
$pending = New-Object System.Collections.Queue
$count = 0
foreach ($raw in Get-Content $manifestPath) {
    $line = $raw.Trim()
    if ($line -eq "" -or $line.StartsWith("#")) { continue }
    $bar = $line.IndexOf("|")
    if ($bar -lt 0) { Log "WARN 跳过无 '|' 的行: $line"; continue }
    $name  = $line.Substring(0, $bar).Trim()
    $flags = $line.Substring($bar + 1).Trim()
    if ($name -notmatch '^[A-Za-z0-9_-]+$') { Log "WARN 跳过非法名字: $name"; continue }
    $pending.Enqueue([pscustomobject]@{ Name = $name; Flags = $flags })
    $count++
}

Log "=== NIGHT_RUN start  tag=$Tag  arms=$count  steps=$Steps  n_envs=$NEnvs  concurrency=$Concurrency ==="
Log "GPU: $(GpuLine)"

$running = New-Object System.Collections.ArrayList
$idx = 0
$done = 0
$lastGpuLog = Get-Date

function Start-Arm($arm) {
    $name    = $arm.Name
    $metrics = Join-Path $proj ("metrics\{0}.jsonl"    -f $name)
    $ckpt    = Join-Path $proj ("checkpoints\{0}.pt"   -f $name)
    $log     = Join-Path $proj ("logs\{0}.log"         -f $name)
    $errlog  = Join-Path $proj ("logs\{0}.err.log"     -f $name)

    # metrics 是追加写：重跑前先删该臂旧产物，保证干净（只删本臂同名文件）
    foreach ($f in @($metrics, $log, $errlog)) { if (Test-Path $f) { Remove-Item $f -Force } }

    $argList = @(
        "--policy","ppo",
        "--n-envs",$NEnvs,
        "--steps",$Steps,
        "--save-path",$ckpt,
        "--save-every",$SaveEvery,
        "--metrics",$metrics,
        "--seed","7"      # 默认 seed；manifest 里若再给 --seed 会覆盖（后出现的 arg 生效）
    )
    $argList += ($arm.Flags -split '\s+')

    $proc = Start-Process -FilePath $exe -ArgumentList $argList `
        -RedirectStandardOutput $log -RedirectStandardError $errlog `
        -WorkingDirectory $proj -PassThru -WindowStyle Hidden
    return [pscustomobject]@{ Name=$name; Proc=$proc; T0=(Get-Date); Metrics=$metrics; Flags=$arm.Flags }
}

while ($pending.Count -gt 0 -or $running.Count -gt 0) {
    # 补位：并发未满 + 还有待跑 + 显存够 → 起新臂
    while ($running.Count -lt $Concurrency -and $pending.Count -gt 0) {
        $free = FreeVram
        if ($free -lt $VramFloorMiB -and $running.Count -ge 1) {
            Log "  显存不足($free MiB < $VramFloorMiB)，暂不补位，等在跑的臂释放"
            break
        }
        $arm = $pending.Dequeue()
        $idx++
        $job = Start-Arm $arm
        [void]$running.Add($job)
        Log (">>> [{0}/{1}] START ARM={2}  pid={3}  flags={4}  (并发 {5}/{6}, GPUfree {7}MiB)" -f `
            $idx, $count, $job.Name, $job.Proc.Id, $job.Flags, $running.Count, $Concurrency, $free)
        Start-Sleep -Seconds 8   # 错峰起，避开同时初始化 CUDA 的尖峰
    }

    # 收割：已退出的臂记账并移出
    for ($i = $running.Count - 1; $i -ge 0; $i--) {
        $job = $running[$i]
        if ($job.Proc.HasExited) {
            $code = $job.Proc.ExitCode
            $mins = [math]::Round(((Get-Date) - $job.T0).TotalMinutes, 1)
            $eps  = 0
            if (Test-Path $job.Metrics) { $eps = (@(Get-Content $job.Metrics -ErrorAction SilentlyContinue)).Count }
            Log ("<<< DONE  ARM={0}  EXIT={1}  ({2} min, {3} episodes)  metrics={4}" -f `
                $job.Name, $code, $mins, $eps, $job.Metrics)
            $running.RemoveAt($i)
            $done++
        }
    }

    # 周期性记 GPU（约每 5 分钟）
    if (((Get-Date) - $lastGpuLog).TotalMinutes -ge 5) {
        Log ("... 进度 done={0}/{1}  在跑={2}  GPU: {3}" -f $done, $count, $running.Count, (GpuLine))
        $lastGpuLog = Get-Date
    }

    Start-Sleep -Seconds 10
}

Log "=== NIGHT_RUN done  tag=$Tag  完成 $done/$count 臂 ==="
