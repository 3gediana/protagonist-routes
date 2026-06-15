param([string]$out="runs\bc_v13.pt",[int]$epochs=8,[string]$nightlist="")
Set-Location 'D:\claude-code\c++\routes\deep_protagonist'
$exe='build\bin\deep_protagonist_train.exe'

# bc_v9 base = 349 files (NO r8 dilution; per Diag-2/round_14)
$dirs = @('runs\demos_d102','runs\demos_d102b','runs\dagger_d102\r4','runs\dagger_d102\r5','runs\dagger_d102\r6','runs\dagger_d102\r7')
$files=@(); foreach($d in $dirs){ $files += (Get-ChildItem "$d\*.bin" -EA SilentlyContinue | Select-Object -Expand FullName) }
$base=$files.Count

# clean night residual bins (fire-free)
$night = @()
if ($nightlist -ne "") { $night = ($nightlist -split ',' | Where-Object { $_ -ne "" }) }

# HARD PRE-TRAIN ASSERTION (decision_round_14 redline): action17==0 over the appended night bins.
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
if ($night.Count -gt 0) {
  python "runs\_fireassert.py" @night
  if ($LASTEXITCODE -ne 0) { "FIRE ASSERTION FAILED (action17>0) -> ABORT, not training" | Tee-Object "runs\bc_v13_train.log"; exit 9 }
}

$files += $night
$total=$files.Count; $nfire=$total-$base
$demos=($files -join ',')
"START $(Get-Date -Format o) base=$base night=$nfire total=$total out=$out" | Tee-Object "runs\bc_v13_train.log"
& $exe --policy bc --bc-demos $demos --bc-out $out `
       --bc-epochs $epochs --bc-batch 16 --bc-bptt 64 --bc-lr 0.0003 `
       --bc-trigger-weight 15 --bc-trigger-oversample 20 `
       *>> "runs\bc_v13_train.log"
"EXIT $LASTEXITCODE $(Get-Date -Format o)" | Tee-Object "runs\bc_v13_train.log" -Append
"TRAINDONE" | Out-File "runs\bc_v13_DONE.txt" -Encoding ASCII
