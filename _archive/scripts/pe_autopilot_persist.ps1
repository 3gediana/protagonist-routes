$ErrorActionPreference = 'Continue'
$pe = 'D:\claude-code\c++\routes\protagonist_ecology'
$exe = 'D:\claude-code\c++\build_ninja_cuda\bin\neural-eco-protagonist-batched.exe'
$summary = Join-Path $pe 'pe_persist_autopilot_summary.txt'
$seeds = @(50022, 50023, 50024)

function Write-Summary($msg) {
    $line = "$(Get-Date -Format s) $msg"
    Add-Content -Path $summary -Value $line
    Write-Output $line
}

function Get-GenLines($seed) {
    $log = Join-Path $pe "onepot_sD_shortrun_$seed.log"
    if (!(Test-Path $log)) { return @() }
    return @(Select-String -Path $log -Pattern 'Gen \d+ \| protagonist.*me_cov=' | ForEach-Object { $_.Line })
}

function Parse-GenLine($line) {
    $gen = [int]([regex]::Match($line, 'Gen (\d+)').Groups[1].Value)
    $cov = [int]([regex]::Match($line, 'me_cov=(\d+)/64').Groups[1].Value)
    $qd = [double]([regex]::Match($line, 'qd=([0-9.]+)').Groups[1].Value)
    $builds = [int]([regex]::Match($line, 'builds=(\d+)').Groups[1].Value)
    $chops = [int]([regex]::Match($line, 'chops=(\d+)').Groups[1].Value)
    $hunts = [int]([regex]::Match($line, 'hunts=(\d+)').Groups[1].Value)
    return [pscustomobject]@{ Gen=$gen; Cov=$cov; Qd=$qd; Builds=$builds; Chops=$chops; Hunts=$hunts; Line=$line }
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

Write-Summary 'autopilot sleeping 3000s before low-frequency check'
Start-Sleep -Seconds 3000

while ($true) {
    $running = @(Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'onepot_sD_shortrun' })
    Write-Summary "running_shortrun_processes=$($running.Count)"
    $pass = $true
    $huntActiveSeeds = 0
    foreach ($seed in $seeds) {
        $lines = Get-GenLines $seed
        if ($lines.Count -eq 0) {
            Write-Summary "seed=$seed no_gen_lines_yet"
            $pass = $false
            continue
        }
        $first = Parse-GenLine $lines[0]
        $last = Parse-GenLine $lines[-1]
        $maxHunts = 0
        foreach ($line in $lines) {
            $p = Parse-GenLine $line
            if ($p.Hunts -gt $maxHunts) { $maxHunts = $p.Hunts }
        }
        if ($maxHunts -gt 0) { $huntActiveSeeds += 1 }
        Write-Summary ("seed={0} gen={1} cov={2}->{3} qd={4}->{5} builds={6} chops={7} hunts_last={8} hunts_max={9}" -f $seed,$last.Gen,$first.Cov,$last.Cov,$first.Qd,$last.Qd,$last.Builds,$last.Chops,$last.Hunts,$maxHunts)
        if ($last.Cov -lt ($first.Cov + 4) -or $last.Qd -le $first.Qd -or $last.Builds -lt 1 -or $last.Chops -lt 1) {
            $pass = $false
        }
    }

    if ($running.Count -gt 0) {
        Write-Summary 'short-run still active; sleeping 3000s'
        Start-Sleep -Seconds 3000
        continue
    }

    if ($huntActiveSeeds -lt 2) {
        Write-Summary "HALT: hunt signal active in only $huntActiveSeeds seeds"
        break
    }

    if (!$pass) {
        Write-Summary 'HALT: short-run failed pass gate'
        break
    }

    Write-Summary 'PASS: launching 100-gen continuation for all seeds'
    foreach ($seed in $seeds) {
        $ckpt = Latest-Checkpoint $seed
        if ($null -eq $ckpt) {
            Write-Summary "HALT: seed=$seed missing checkpoint"
            $pass = $false
            break
        }
        $src = Join-Path $pe "onepot_sD_shortrun_$seed.toml"
        $dst = Join-Path $pe "onepot_sD_longrun_$seed.toml"
        $text = Get-Content $src -Raw
        $text = $text -replace 'scenario_name = "[^"]+"', ('scenario_name = "onepot_sD_longrun_{0}"' -f $seed)
        $text = $text -replace 'resume_checkpoint_path = "[^"]+"', ('resume_checkpoint_path = "{0}"' -f ($ckpt.Path -replace '\\','/'))
        $text = $text -replace '(?m)^generations = \d+', ('generations = {0}' -f ($ckpt.Gen + 100))
        $text = $text -replace '(?m)^checkpoint_every = \d+', 'checkpoint_every = 10'
        Set-Content -Path $dst -Value $text -Encoding UTF8
        $log = Join-Path $pe "onepot_sD_longrun_$seed.log"
        $cfg = "onepot_sD_longrun_$seed.toml"
        $cmd = "cmd /c `"cd /d `"$pe`" && set PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%&& `"$exe`" --config $cfg > `"$log`" 2>&1`""
        $p = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = $cmd }
        Write-Summary "launched_longrun seed=$seed pid=$($p.ProcessId) from_gen=$($ckpt.Gen)"
        Start-Sleep -Seconds 5
    }
    break
}
