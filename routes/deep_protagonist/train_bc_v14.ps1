param([string]$out="runs\bc_v14.pt",[int]$epochs=8,[int]$lowos=8)
Set-Location 'D:\claude-code\c++\routes\deep_protagonist'
$exe='build\bin\deep_protagonist_train.exe'

# bc_v9 base = 349 files (NO r8 dilution; per Diag-2/round_14) -- kept at global os20 (bc_v9 recipe)
$dirs = @('runs\demos_d102','runs\demos_d102b','runs\dagger_d102\r4','runs\dagger_d102\r5','runs\dagger_d102\r6','runs\dagger_d102\r7')
$files=@(); foreach($d in $dirs){ $files += (Get-ChildItem "$d\*.bin" -EA SilentlyContinue | Select-Object -Expand FullName) }
$base=$files.Count

# round_16 / decision_round_15 Path A: only the 2 HARDEST fire-free night bins (nShel~0.30),
# downweighted via a night-specific low trigger-oversample channel (base daytime/build untouched).
$night = @("runs\night_dagger_ff\night_2300105.bin","runs\night_dagger_ff\night_2300148.bin")

# HARD PRE-TRAIN ASSERTION (decision redline): action17==0 over the appended night bins.
$auditcmd = "import struct,sys`n"+
"tot=0`n"+
"for p in sys.argv[1:]:`n"+
"    d=open(p,'rb').read()`n"+
"    assert d[:4]==b'DPB1', p`n"+
"    _,od,cd=struct.unpack_from('<III',d,4); rec=od*4+cd*4+8; b=d[24:]; n=len(b)//rec; off=od*4+cd*4`n"+
"    c=sum(1 for i in range(n) if struct.unpack_from('<I',b,i*rec+off)[0]==17)`n"+
"    tot+=c; print(f'{p}: a17={c}')`n"+
"print('TOTAL_A17='+str(tot))`n"+
"sys.exit(1 if tot>0 else 0)`n"
$auditcmd | Out-File "runs\_fireassert.py" -Encoding ASCII
python "runs\_fireassert.py" @night
if ($LASTEXITCODE -ne 0) { "FIRE ASSERTION FAILED (action17>0) -> ABORT, not training" | Tee-Object "runs\bc_v14_train.log"; exit 9 }

$files += $night
$total=$files.Count; $nnight=$total-$base
$demos=($files -join ',')
$lowdemos=($night -join ',')
"START $(Get-Date -Format o) base=$base night=$nnight total=$total lowos=$lowos out=$out" | Tee-Object "runs\bc_v14_train.log"
& $exe --policy bc --bc-demos $demos --bc-out $out `
       --bc-epochs $epochs --bc-batch 16 --bc-bptt 64 --bc-lr 0.0003 `
       --bc-trigger-weight 15 --bc-trigger-oversample 20 `
       --bc-low-os $lowos --bc-low-os-demos $lowdemos `
       *>> "runs\bc_v14_train.log"
"EXIT $LASTEXITCODE $(Get-Date -Format o)" | Tee-Object "runs\bc_v14_train.log" -Append
"TRAINDONE" | Out-File "runs\bc_v14_DONE.txt" -Encoding ASCII
