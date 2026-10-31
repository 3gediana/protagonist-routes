param([string]$parent)
cd D:\claude-code\c++
$d = Get-ChildItem $parent -Directory | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty FullName
Write-Host ("RUNDIR=" + $d)
python scripts/protagonist_harmony_check.py $d
