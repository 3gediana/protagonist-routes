$ErrorActionPreference = 'Continue'
$pe = 'D:\claude-code\c++\routes\protagonist_ecology'
$exe = 'D:\claude-code\c++\build_ninja_cuda\bin\neural-eco-protagonist-batched.exe'
$summary = Join-Path $pe 'pe_parallel_hypotheses_summary.txt'
$seeds = @(50022, 50023, 50024)
$families = @(
    @{ Name='explore';  Line='0.10'; Mut='1.50'; Hunt='3'; Cvt='64';  Gens=60 },
    @{ Name='huntwide'; Line='0.05'; Mut='1.00'; Hunt='5'; Cvt='96';  Gens=60 },
    @{ Name='hybrid';   Line='0.08'; Mut='1.25'; Hunt='5'; Cvt='96';  Gens=60 }
)

function Write-Summary($msg) {
    $line = "$(Get-Date -Format s) $msg"
    Add-Content -Path $summary -Value $line
    Write-Output $line
}

function Latest-Checkpoint($seed) {
    $base = Join-Path $pe "data\runs\onepot_sD_warm_$seed"
    $best = $null
    Get-ChildItem $base -Directory -ErrorAction SilentlyContinue | ForEach-Object {
        Get-ChildItem $_.FullName -Directory -Filter 'checkpoint_gen*' -ErrorAction SilentlyContinue | ForEach-Object {
            $m = [regex]::Match($_.Name, 'checkpoint_gen(\d+)')
            if ($m.Success) {
                $gen = [int]$m.Groups[1].Value
                if ($null -eq $best -or $gen -gt $best.Gen) {
                    $best = [pscustomobject]@{ Gen=$gen; Path=$_.FullName }
                }
            }
        }
    }
    return $best
}

function Patch-Or-Append($text, $pattern, $replacement, $anchorPattern) {
    if ($text -match $pattern) {
        return ($text -replace $pattern, $replacement)
    }
    if ($text -match $anchorPattern) {
        return ($text -replace $anchorPattern, ("`$0`r`n" + $replacement))
    }
    return ($text + "`r`n" + $replacement + "`r`n")
}

Set-Content -Path $summary -Value "parallel hypotheses launched $(Get-Date -Format s)"
foreach ($seed in $seeds) {
    $ckpt = Latest-Checkpoint $seed
    if ($null -eq $ckpt) {
        Write-Summary "SKIP seed=$seed missing checkpoint"
        continue
    }
    $template = Join-Path $pe "onepot_sD_shortrun_$seed.toml"
    if (!(Test-Path $template)) {
        Write-Summary "SKIP seed=$seed missing template $template"
        continue
    }
    foreach ($family in $families) {
        $name = $family.Name
        $cfgName = "onepot_sD_${name}_$seed.toml"
        $cfgPath = Join-Path $pe $cfgName
        $text = Get-Content $template -Raw
        $text = $text -replace 'scenario_name = "[^"]+"', ('scenario_name = "onepot_sD_{0}_{1}"' -f $name, $seed)
        $text = $text -replace 'resume_checkpoint_path = "[^"]+"', ('resume_checkpoint_path = "{0}"' -f ($ckpt.Path -replace '\\','/'))
        $text = $text -replace '(?m)^generations = \d+', ('generations = {0}' -f ($ckpt.Gen + [int]$family.Gens))
        $text = $text -replace '(?m)^checkpoint_every = \d+', 'checkpoint_every = 10'
        $text = $text -replace '(?m)^line_sigma = [0-9.]+', ('line_sigma = {0}' -f $family.Line)
        $text = $text -replace '(?m)^mutation_boost = [0-9.]+', ('mutation_boost = {0}' -f $family.Mut)
        $text = $text -replace '(?m)^hunt_max_bins = \d+', ('hunt_max_bins = {0}' -f $family.Hunt)
        $text = $text -replace '(?m)^cvt_num_centroids = \d+', ('cvt_num_centroids = {0}' -f $family.Cvt)
        $text = Patch-Or-Append $text '(?m)^lifetime_archive_enabled = \w+' 'lifetime_archive_enabled = true' '(?m)^cvt_kmeans_iters = \d+'
        Set-Content -Path $cfgPath -Value $text -Encoding UTF8

        $log = Join-Path $pe "onepot_sD_${name}_$seed.log"
        $cmd = "cmd /c `"cd /d `"$pe`" && set PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%&& `"$exe`" --config $cfgName > `"$log`" 2>&1`""
        $p = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = $cmd }
        Write-Summary ("launch family={0} seed={1} pid={2} from_gen={3} line={4} mut={5} hunt_bins={6} cvt={7}" -f $name,$seed,$p.ProcessId,$ckpt.Gen,$family.Line,$family.Mut,$family.Hunt,$family.Cvt)
        Start-Sleep -Seconds 3
    }
}

Start-Sleep -Seconds 20
$running = @(Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'onepot_sD_(explore|huntwide|hybrid|short|long)run|onepot_sD_(explore|huntwide|hybrid)_' })
Write-Summary "running_matching_processes=$($running.Count)"
try {
    $gpu = (& nvidia-smi --query-gpu=memory.used,memory.total,utilization.gpu --format=csv,noheader,nounits) -join ' '
    Write-Summary "gpu=$gpu"
} catch {
    Write-Summary "gpu_query_failed=$($_.Exception.Message)"
}

Write-Summary 'sleeping 3000s before next summary'
Start-Sleep -Seconds 3000
Write-Summary 'woke after 3000s; summarize manually or relaunch monitor if needed'
