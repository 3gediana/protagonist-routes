<#
DP parallel hypotheses — run multiple PBRS/config variants from ft_gen.pt
Since GPU mem is limited (~2GB free), run them sequentially-ish or with small n-envs
Each is a 2M smoke; we'll compare unlock counts after completion
#>
$ErrorActionPreference = 'Stop'
$dp  = 'D:\claude-code\c++\routes\deep_protagonist'
$exe = "$dp\build_d122_v2\bin\deep_protagonist_train.exe"
Set-Location $dp

# Check if exe exists
if(-not (Test-Path $exe)) {
    Write-Output "ERROR: $exe not found"
    exit 1
}

$hypotheses = @(
    @{
        tag='hyp_entropy'
        desc='High entropy exploration'
        repShap=1.5; huntShap=0.3; warmth=1.0; ent=0.05; lr='2.0e-4'; nenvs=2; steps=2000000
    },
    @{
        tag='hyp_huntfocus'
        desc='Hunt-focused shaping'
        repShap=1.0; huntShap=0.8; warmth=0.8; ent=0.02; lr='1.5e-4'; nenvs=2; steps=2000000
    },
    @{
        tag='hyp_aggressive'
        desc='Aggressive all-shaping'
        repShap=2.5; huntShap=0.6; warmth=1.5; ent=0.03; lr='1.5e-4'; nenvs=2; steps=2000000
    }
)

$logFile = "$dp\runs\dp_hypotheses_log.txt"
$timestamp = Get-Date -Format 'yyyy-MM-ddTHH:mm:ss'

foreach($h in $hypotheses) {
    $tag = $h.tag
    $out = "$dp\runs\ft_$tag.stdout.log"
    $err = "$dp\runs\ft_$tag.stderr.log"
    Remove-Item $out,$err -ErrorAction SilentlyContinue
    
    $argline = "--policy ppo --load runs\ft_gen.pt --weather --allow-cook " +
               "--ent-coef $($h.ent) --critic-warmup 32 --lr $($h.lr) --ppo-target-kl 0.08 " +
               "--ppo-hunt-shaping $($h.huntShap) --ppo-repertoire-shaping $($h.repShap) --ppo-warmth-shaping $($h.warmth) " +
               "--n-envs $($h.nenvs) --steps $($h.steps) --save-path runs\ft_$tag.pt --save-every 25 " +
               "--metrics runs\ft_$tag.jsonl --seed 24"
    
    $proc = Start-Process $exe -ArgumentList $argline -RedirectStandardOutput $out -RedirectStandardError $err -PassThru -NoNewWindow
    $msg = "$timestamp launch dp_hyp=$tag pid=$($proc.Id) desc='$($h.desc)' rep=$($h.repShap) hunt=$($h.huntShap) warmth=$($h.warmth) ent=$($h.ent)"
    Write-Output $msg
    Add-Content $logFile $msg
    Start-Sleep 3
}

# Also check current GPU after launches
Start-Sleep 10
$gpu = nvidia-smi --query-gpu=memory.used,memory.total,utilization.gpu --format=csv,noheader,nounits
Write-Output "gpu_after_launch=$gpu"
Add-Content $logFile "gpu_after_launch=$gpu"

# Count total running processes
$procs = Get-CimInstance Win32_Process | Where-Object { $_.Name -match 'neural-eco|deep_protagonist' }
Write-Output "total_training_processes=$($procs.Count)"
Add-Content $logFile "total_training_processes=$($procs.Count)"
