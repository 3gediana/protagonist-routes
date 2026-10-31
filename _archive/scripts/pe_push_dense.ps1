# PE density push: 50025 config -> 50021-24 + restart all 5 seeds
$ErrorActionPreference = 'Stop'
$pe = 'D:\claude-code\c++\routes\protagonist_ecology'
Set-Location $pe

# --- Stop existing PE processes ---
Write-Output "=== Stopping existing PE ==="
$procs = Get-Process -Name '*neural-eco-protagonist*' -ErrorAction SilentlyContinue
if ($procs) {
    $procs | Stop-Process -Force
    Write-Output ("Stopped " + $procs.Count + " processes")
    Start-Sleep -Seconds 3
} else {
    Write-Output "No PE processes running"
}

# --- Patch 50021-24 with density config ---
foreach ($seed in @(50021, 50022, 50023, 50024)) {
    $toml = "$pe\onepot_sD_warm_$seed.toml"
    if (-Not (Test-Path $toml)) { Write-Output "SKIP $toml not found"; continue }
    $lines = Get-Content $toml
    $inTreeSection = $false
    $treeCountDone = $false
    $newlines = @()
    foreach ($line in $lines) {
        # Track when we enter [world.tree]
        if ($line -match '^\[world\.tree\]') { $inTreeSection = $true }
        elseif ($line -match '^\[' -and $inTreeSection) { $inTreeSection = $false }
        
        # In [world.tree], replace count = 80 with count = 240
        if ($inTreeSection -and -not $treeCountDone -and $line -match '^count\s*=') {
            $newlines += 'count = 240'
            $treeCountDone = $true
            continue
        }
        # Replace thresholds anywhere
        if ($line -match '^worksite_unlock_deposit_threshold\s*=') {
            $newlines += 'worksite_unlock_deposit_threshold = 4'
            continue
        }
        if ($line -match '^stone_unlock_deposit_threshold\s*=') {
            $newlines += 'stone_unlock_deposit_threshold = 3'
            continue
        }
        # Bump generations
        if ($line -match '^generations\s*=\s*\d+') {
            $newlines += 'generations = 440'
            continue
        }
        $newlines += $line
    }
    $newlines -join "`n" | Set-Content -Path $toml -NoNewline
    Write-Output "Patched $seed (tree=240, worksite=4, stone=3, gens=440)"
}

# --- Extend 50025 generations if needed ---
$t25 = "$pe\onepot_sD_warm_50025.toml"
$c25 = Get-Content $t25 -Raw
if ($c25 -match 'generations\s*=\s*(\d+)') {
    $g = [int]$Matches[1]
    if ($g -lt 440) {
        $c25 = $c25 -replace '(?m)^generations\s*=\s*\d+', 'generations = 440'
        Set-Content -Path $t25 -Value $c25 -NoNewline
        Write-Output "Extended 50025 generations $g -> 440"
    } else {
        Write-Output "50025 already at $g generations"
    }
}

# --- Verify patches ---
Write-Output "`n=== Verify patches ==="
foreach ($seed in @(50021, 50022, 50023, 50024, 50025)) {
    $c = Get-Content "$pe\onepot_sD_warm_$seed.toml" -Raw
    $wt = if ($c -match 'worksite_unlock_deposit_threshold\s*=\s*(\d+)') { $Matches[1] } else { '?' }
    $st = if ($c -match 'stone_unlock_deposit_threshold\s*=\s*(\d+)') { $Matches[1] } else { '?' }
    $tc = ''
    $lines2 = Get-Content "$pe\onepot_sD_warm_$seed.toml"
    $inT = $false
    foreach ($l in $lines2) {
        if ($l -match '^\[world\.tree\]') { $inT = $true; continue }
        if ($l -match '^\[' -and $inT) { break }
        if ($inT -and $l -match '^count\s*=\s*(\d+)') { $tc = $Matches[1]; break }
    }
    $gen = if ($c -match '(?m)^generations\s*=\s*(\d+)') { $Matches[1] } else { '?' }
    Write-Output "${seed}: tree=$tc worksite=$wt stone=$st gens=$gen"
}

# --- Launch all 5 seeds ---
Write-Output "`n=== Launching PE seeds ==="
$exe = 'D:\claude-code\c++\build_ninja_cuda\bin\neural-eco-protagonist-batched.exe'
foreach ($seed in @(50021, 50022, 50023, 50024, 50025)) {
    $toml = "onepot_sD_warm_$seed.toml"
    $log  = "$pe\onepot_sD_warm_$seed.log"
    $cmdline = 'cmd /c "set PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%&& "' + $exe + '" --config ' + $toml + ' > "' + $log + '" 2>&1"'
    $r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine=$cmdline; CurrentDirectory=$pe }
    Write-Output ("seed=$seed PID=" + $r.ProcessId + " RV=" + $r.ReturnValue)
    Start-Sleep -Seconds 2
}

Write-Output "`n=== Waiting for processes to stabilize ==="
Start-Sleep -Seconds 8
$running = Get-Process -Name '*neural-eco*' -ErrorAction SilentlyContinue
Write-Output ("Running PE processes: " + $(if ($running) { $running.Count } else { 0 }))
if ($running) { $running | ForEach-Object { Write-Output ("  PID=" + $_.Id + " WS=" + [math]::Round($_.WorkingSet64/1MB) + "MB") } }
