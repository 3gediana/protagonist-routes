$ErrorActionPreference = 'Continue'
$r = 'D:/claude-code/c++/routes/deep_protagonist'
Set-Location $r
$exe = "$r/build/bin/deep_protagonist_train.exe"
$steps = 18000
$seed  = 2100000

function RunOne($name, $extra, $metrics) {
    if (Test-Path $metrics) { Remove-Item $metrics }
    $args = @('--steps', $steps, '--seed', $seed, '--metrics', $metrics) + $extra
    & $exe @args *> "runs/_gate_$name.log"
    Write-Output ("[$name] EXE_EXIT=" + $LASTEXITCODE)
    Select-String -Path "runs/_gate_$name.log" -Pattern 'warmstart surgery|checkpoint loaded' | ForEach-Object { '   ' + $_.Line }
}

# 1) settler oracle (forage + hunt) — nutrition ON  => should keep both bars alive
RunOne 'settler'   @('--policy','settler','--nutrition')                                   'runs/n_settler.jsonl'
# 2) noop (lazy) — nutrition ON => both bars starve, deaths accumulate
RunOne 'noop'      @('--policy','noop','--nutrition')                                      'runs/n_noop.jsonl'
# 3) bc_v9 surgery, nutrition ON => forages (vitamin ok) but no hunting (protein starves)
RunOne 'bcv9on'    @('--policy','ppo','--load','runs/bc_v9.pt','--no-update','--deterministic','--ppo-mask-fire-only','--nutrition') 'runs/n_bcv9on.jsonl'
# 4) bc_v9 surgery, nutrition OFF => behaviour-identity check vs old world (atk=0, builds, shelters)
RunOne 'bcv9off'   @('--policy','ppo','--load','runs/bc_v9.pt','--no-update','--deterministic','--ppo-mask-fire-only') 'runs/n_bcv9off.jsonl'

Write-Output 'DONE_GATE'
